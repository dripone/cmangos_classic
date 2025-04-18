/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Boss_Cthun
SD%Complete: 95
SDComment: Transform spell has some minor core issues. Digestive Acid should be handled in core. Spell to kill players in stomach on reset needs implementation.
SDCategory: Temple of Ahn'Qiraj
EndScriptData

*/

#include "AI/ScriptDevAI/base/CombatAI.h"
#include "AI/ScriptDevAI/include/sc_common.h"
#include "temple_of_ahnqiraj.h"

enum
{
    EMOTE_WEAKENED                  = 11476,

    SPELL_CHECK_RESET_AURA          = 26255,
    SPELL_RESET_ENCOUNTER           = 26257,

    // ***** Phase 1 ********
    SPELL_EYE_BEAM                  = 26134,
    SPELL_EYE_BEAM_INIT             = 32950,                // Cast three times on victim at encounter start, then replaced by spell 26134 on random targets
    SPELL_DARK_GLARE                = 26029,
    SPELL_ROTATE_TRIGGER            = 26137,                // phase switch spell - triggers 26009 or 26136. These trigger the Dark Glare spell - 26029
    SPELL_ROTATE_360_LEFT           = 26009,
    SPELL_ROTATE_360_RIGHT          = 26136,

    SPELL_SUMMON_HOOK_TENTACLES_1   = 26397,                // Periodically triggers spell 26398 which cast spell 26140 to summon NPC 15725
    SPELL_SUMMON_HOOK_TENTACLES_2   = 26398,
    SPELL_SUMMON_HOOK_TENTACLE      = 26140,                // Summon NPC 15725

    SPELL_SUMMON_EYE_TENTACLES      = 26152,
    SPELL_SUMMON_EYE_TENTACLE_1     = 26144,
    SPELL_SUMMON_EYE_TENTACLE_2     = 26145,
    SPELL_SUMMON_EYE_TENTACLE_3     = 26146,
    SPELL_SUMMON_EYE_TENTACLE_4     = 26147,
    SPELL_SUMMON_EYE_TENTACLE_5     = 26148,
    SPELL_SUMMON_EYE_TENTACLE_6     = 26149,
    SPELL_SUMMON_EYE_TENTACLE_7     = 26150,
    SPELL_SUMMON_EYE_TENTACLE_8     = 26151,

    SPELL_DESPAWN_TENTACLES         = 26399,

    SPELL_INVIS_AND_STEALTH_DETECT  = 18950,

    // ***** Phase 2 ******
    SPELL_CARAPACE_CTHUN            = 26156,                // Was removed from client dbcs
    SPELL_TRANSFORM                 = 26232,
    SPELL_CTHUN_VULNERABLE          = 26235,
    SPELL_SUMMON_EYE_TENTACLES_P2   = 26769,
    SPELL_GIANT_EYE_TENTACLES_1     = 26766,                // Periodically triggers 26767 that cast 26768 on random target to summon NPC 15334
    SPELL_GIANT_EYE_TENTACLES_2     = 26767,
    SPELL_SUMMON_GIANT_EYE_TENTACLE = 26768,                // Summon NPC 15334
    SPELL_SUMMON_GIANT_HOOKS_1      = 26213,                // Periodically triggers 26217 that cast 26216 on random target to summon NPC 15728
    SPELL_SUMMON_GIANT_HOOKS_2      = 26217,
    SPELL_SUMMON_GIANT_HOOK_TENTACLE= 26216,                // Summon NPC 15728
    SPELL_SUMMON_MOUTH_TENTACLES_1  = 26236,                // Periodically triggers 26237 that cast 26332 on random target to summon NPC 15910
    SPELL_SUMMON_MOUTH_TENTACLES_2  = 26237,
    SPELL_SUMMON_MOUTH_TENTACLE     = 26332,                // Summon NPC 15910 and prepare target to be teleported to stomach
    SPELL_DIGESTIVE_ACID_TELEPORT   = 26220,                // Stomach teleport spell
    SPELL_EXIT_STOMACH_EFFECT       = 26230,                // Complete the eject effect from the stomach towards a random location in the room
    SPELL_PORT_OUT_STOMACH_EFFECT   = 26648,                // used to kill players inside the stomach on evade
    SPELL_DIGESTIVE_ACID            = 26476,                // damage spell - should be handled by the map
    SPELL_EXIT_STOMACH              = 26221,                // Summon NPC 15800 that will handle the initial knockback outside the stomach through spell 26224 (EventAI)

    NPC_WORLD_TRIGGER               = 15384,

    DISPLAY_ID_CTHUN_BODY           = 15786,                // Helper display id; This is needed in order to have the proper transform animation. ToDo: remove this when auras are fixed in core.

    AREATRIGGER_STOMACH_1           = 4033,
    AREATRIGGER_STOMACH_2           = 4034,
    AREATRIGGER_STOMACH_3           = 4036,

    MAX_FLESH_TENTACLES             = 2,
};

static const float cthunLocations[4][4] =
{
    { -8571.472f, 1990.2048f, -98.777855f, 1.01229095458984375f}, // Flesh Tentacles locations
    { -8525.083f, 1994.7125f, -98.255325f, 2.58308720588684082f},
    { -8563.56f, 2040.69f,   -97.f,  4.9426f},          // Stomach teleport location (in)
    { -8576.06f, 1985.8359f, 100.22033f, 6.108652f},    // Stomach eject location (out)
};

/*######
## boss_eye_of_cthun
######*/

enum EyeCthunActions
{
    EYECTHUN_INIT_EYEBEAM,
    EYECTHUN_ACTION_MAX,
    EYECTHUN_END_DARKGLARE,
};

struct boss_eye_of_cthunAI : public CombatAI
{
    boss_eye_of_cthunAI(Creature* creature) : CombatAI(creature, EYECTHUN_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCombatAction(EYECTHUN_INIT_EYEBEAM, 1u); // immediately on first combat tick
        AddCustomAction(EYECTHUN_END_DARKGLARE, true, [&]() { HandleEndDarkGlare(); });
    }

    ScriptedInstance* m_instance;

    uint8 m_eyeBeamCount;

    void Reset() override
    {
        CombatAI::Reset();

        SetMeleeEnabled(false);
        SetCombatScriptStatus(false);
        SetCombatMovement(false);

        m_eyeBeamCount = 0;
    }

    void JustRespawned() override
    {
        CombatAI::JustRespawned();
        DoCastSpellIfCan(nullptr, SPELL_INVIS_AND_STEALTH_DETECT);
    }

    void Aggro(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_CTHUN, IN_PROGRESS);

        m_creature->SetInCombatWithZone();
        AttackClosestEnemy();

        // Start periodically summoning Eye and Claw (Hook) Tentacles
        DoCastSpellIfCan(nullptr, SPELL_SUMMON_EYE_TENTACLES, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT );
        DoCastSpellIfCan(nullptr, SPELL_SUMMON_HOOK_TENTACLES_1, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoCastSpellIfCan(nullptr, SPELL_CHECK_RESET_AURA, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // Allow the body to begin the transition (internal 5 secs delay)
        if (m_instance)
        {
            if (Creature* cthun = m_instance->GetSingleCreatureFromStorage(NPC_CTHUN))
                m_creature->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, m_creature, cthun);
        }
    }

    void EnterEvadeMode() override
    {
        // Despawn all tentacles and portals
        DoCastSpellIfCan(nullptr, SPELL_DESPAWN_TENTACLES, TRIGGERED_OLD_TRIGGERED);

        m_creature->RemoveAurasDueToSpell(SPELL_SUMMON_HOOK_TENTACLES_1);
        m_creature->RemoveAurasDueToSpell(SPELL_SUMMON_EYE_TENTACLES);
        m_creature->RemoveAurasDueToSpell(SPELL_CHECK_RESET_AURA);

        m_creature->SetRespawnDelay(60, true);
        m_creature->ForcedDespawn();

        if (m_instance)
            m_instance->SetData(TYPE_CTHUN, FAIL);
    }

    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_CLAW_TENTACLE:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    summoned->AI()->AttackStart(target);
                break;
        }
    }

    void HandleEndDarkGlare()
    {
        // Remove rotation auras
        m_creature->RemoveAurasDueToSpell(SPELL_ROTATE_360_LEFT);
        m_creature->RemoveAurasDueToSpell(SPELL_ROTATE_360_RIGHT);

        // Switch to Eye Beam and resume targeting
        SetCombatScriptStatus(false);
        if (m_creature->GetVictim())
            m_creature->SetTarget(m_creature->GetVictim());

        AddInitialCooldowns();
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        if (spellInfo->Id == SPELL_ROTATE_TRIGGER)
        {
            // We ignore targets during Dark Glare to control Eye of C'Thun's orientation ourselves in spells
            // So we need custom AI management for this sub phase
            SetCombatScriptStatus(true);
            m_creature->SetTarget(nullptr);

            // Switch to Dark Glare phase for 40 seconds
            ResetTimer(EYECTHUN_END_DARKGLARE, 40000);
        }
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case EYECTHUN_INIT_EYEBEAM:
                // The first three casts of "Eye Beam" use a specific spell that always targets Eye of C'Thun's victim
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_EYE_BEAM_INIT) == CAST_OK)
                {
                    ++m_eyeBeamCount;
                    if (m_eyeBeamCount == 3)
                        DisableCombatAction(action);
                    else
                        ResetCombatAction(action, urand(3000, 5000));
                }
                break;
        }

    }
};

/*######
## boss_cthun
######*/

enum CthunActions
{
    CTHUN_CLAWTENTACLEDELAY,
    CTHUN_EYETENTACLEDELAY,
    CTHUN_ACTION_MAX,
    CTHUN_EMERGE,
    CTHUN_WEAKENED_END,
};

struct boss_cthunAI : public CombatAI
{
    boss_cthunAI(Creature* creature) : CombatAI(creature, CTHUN_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        // There are 30 seconds offset between Giant Eye and Giant Claw Tentacles which are both on a 60 seconds period handled by periodic spells
        AddCombatAction(CTHUN_CLAWTENTACLEDELAY, 10000u);
        AddCombatAction(CTHUN_EYETENTACLEDELAY, 40000u);
        AddCustomAction(CTHUN_EMERGE, true, [&]() { HandleEmerge(); }, TIMER_ALWAYS);
        AddCustomAction(CTHUN_WEAKENED_END, true, [&]() { HandleEndWeaken(); }, TIMER_COMBAT_COMBAT);
        // Set active in order to be used during the instance progress
        m_creature->SetActiveObjectState(true);
    }

    ScriptedInstance* m_instance;

    // Global variables
    uint8 m_fleshTentaclesKilled;

    // Body Phase

    GuidList m_playersInStomachList;

    ObjectGuid m_stomachEnterTargetGuid;

    void Reset() override
    {
        CombatAI::Reset();

        m_fleshTentaclesKilled    = 0;

        // Clear players in stomach
        m_playersInStomachList.clear();

        // Reset flags
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
        SetCombatMovement(false);
        SetMeleeEnabled(false);
        SetReactState(REACT_PASSIVE);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override
    {
        if (eventType == AI_EVENT_CUSTOM_A)
            ResetTimer(CTHUN_EMERGE, 5000);
    }

    void Aggro(Unit* /*who*/) override
    {
        m_creature->SetInCombatWithZone();
        AttackClosestEnemy();

        // Start periodically summoning Eye, Giant Eye and Mouth Tentacles
        DoSpawnTentacles();
        DoCastSpellIfCan(nullptr, SPELL_CHECK_RESET_AURA, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void EnterEvadeMode() override
    {
        // Kill any player from the stomach on evade - this is becuase C'thun cannot be soloed.
        for (auto guid : m_playersInStomachList)
        {
            if (Player* player = m_creature->GetMap()->GetPlayer(guid))
                player->CastSpell(nullptr, SPELL_PORT_OUT_STOMACH_EFFECT, TRIGGERED_OLD_TRIGGERED);
        }

        m_creature->RemoveAurasDueToSpell(SPELL_CHECK_RESET_AURA);
        StopSpawningTentacles();

        m_creature->SetRespawnDelay(60, true);
        m_creature->ForcedDespawn();

        if (m_instance)
            m_instance->SetData(TYPE_CTHUN, FAIL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        m_creature->SetActiveObjectState(false);

        DoCastSpellIfCan(nullptr, SPELL_DESPAWN_TENTACLES, TRIGGERED_OLD_TRIGGERED);

        if (m_instance)
            m_instance->SetData(TYPE_CTHUN, DONE);
    }

    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_GIANT_EYE_TENTACLE:
            case NPC_GIANT_CLAW_TENTACLE:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    summoned->AI()->AttackStart(target);
                break;
        }
    }

    void SummonedCreatureJustDied(Creature* summoned) override
    {
        // Handle Flesh Tentacle kill in stomach
        if (summoned->GetEntry() == NPC_FLESH_TENTACLE)
        {
            ++m_fleshTentaclesKilled;
            if (m_fleshTentaclesKilled == MAX_FLESH_TENTACLES)
                HandleWeaken();
        }
    }

    void DoSpawnTentacles()
    {
        // Tentacles Party... Pleasure !
        DoCastSpellIfCan(nullptr, SPELL_SUMMON_EYE_TENTACLES_P2, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT );
        DoCastSpellIfCan(nullptr, SPELL_SUMMON_MOUTH_TENTACLES_1, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT );
        ResetCombatAction(CTHUN_CLAWTENTACLEDELAY, 10000);
        ResetCombatAction(CTHUN_EYETENTACLEDELAY, 40000);

        // Flesh Tentacles inside stomach, two of them
        m_fleshTentaclesKilled = 0;
        for (uint8 i = 0; i < MAX_FLESH_TENTACLES; ++i)
            m_creature->SummonCreature(NPC_FLESH_TENTACLE, cthunLocations[i][0], cthunLocations[i][1], cthunLocations[i][2], cthunLocations[i][3], TEMPSPAWN_DEAD_DESPAWN, 0);
    }

    void StopSpawningTentacles()
    {
        // End of the tentacles everywhere. No more pleasure.
        m_creature->RemoveAurasDueToSpell(SPELL_GIANT_EYE_TENTACLES_1);
        m_creature->RemoveAurasDueToSpell(SPELL_SUMMON_EYE_TENTACLES_P2);
        m_creature->RemoveAurasDueToSpell(SPELL_SUMMON_GIANT_HOOKS_1);
        m_creature->RemoveAurasDueToSpell(SPELL_SUMMON_MOUTH_TENTACLES_1);
    }

    void HandleEmerge()
    {
        // Transform and start C'Thun phase
        if (DoCastSpellIfCan(nullptr, SPELL_TRANSFORM) == CAST_OK)
        {
            // Make ready for fight
            SetReactState(REACT_AGGRESSIVE);
            DoCastSpellIfCan(nullptr, SPELL_CARAPACE_CTHUN, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
            m_creature->SetInCombatWithZone();
        }
    }

    void HandleWeaken()
    {
        if (DoCastSpellIfCan(nullptr, SPELL_CTHUN_VULNERABLE, CAST_INTERRUPT_PREVIOUS) == CAST_OK)
        {
            m_creature->RemoveAurasDueToSpell(SPELL_CHECK_RESET_AURA);
            m_creature->RemoveAurasDueToSpell(SPELL_CARAPACE_CTHUN);
            DoBroadcastText(EMOTE_WEAKENED, m_creature);
            StopSpawningTentacles();
            ResetTimer(CTHUN_WEAKENED_END, 45 * IN_MILLISECONDS);
        }
    }

    void HandleEndWeaken()
    {
        // Handle carapace and tentacles respawn when the vulnerability spell expires
        DoCastSpellIfCan(nullptr, SPELL_CARAPACE_CTHUN, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoCastSpellIfCan(nullptr, SPELL_CHECK_RESET_AURA, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoSpawnTentacles();
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case CTHUN_CLAWTENTACLEDELAY:
                // The first Giant Claw Tentacle (at phase 2 start of after weakened state) is a single isolated cast,
                // all after are on periodic timer through dedicated spell
                // Check for valid player
                if (Unit* player = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, uint32(0), SELECT_FLAG_IN_LOS | SELECT_FLAG_PLAYER))
                {
                    player->CastSpell(nullptr, SPELL_SUMMON_GIANT_HOOK_TENTACLE, TRIGGERED_OLD_TRIGGERED);
                    // All further summons are handled in periodic spell SPELL_SUMMON_GIANT_HOOKS_1
                    if (DoCastSpellIfCan(nullptr, SPELL_SUMMON_GIANT_HOOKS_1, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT ) == CAST_OK)
                        DisableCombatAction(action);
                    // In case of failure, try another single cast
                    else
                        ResetCombatAction(action, 30000);
                }
                break;
            case CTHUN_EYETENTACLEDELAY:
                // The first Giant Eye Tentacle (at phase 2 start of after weakened state, 30 after first Giant Claw Tentacle) is a single isolated cast,
                // all after are on periodic timer through dedicated spell
                // Check for valid player
                if (Unit* player = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, uint32(0), SELECT_FLAG_IN_LOS | SELECT_FLAG_PLAYER))
                {
                    player->CastSpell(nullptr, SPELL_SUMMON_GIANT_EYE_TENTACLE, TRIGGERED_OLD_TRIGGERED);
                    // All further summons are handled in periodic spell SPELL_GIANT_EYE_TENTACLES_1
                    if (DoCastSpellIfCan(nullptr, SPELL_GIANT_EYE_TENTACLES_1, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT ) == CAST_OK)
                        DisableCombatAction(action);
                    // In case of failure, try another single cast
                    else
                        ResetCombatAction(action, 30000);
                }
                break;
            default:
                break;
        }
    }
};

/*######
## npc_giant_claw_tentacle
######*/

enum
{
    SPELL_GIANT_GROUND_RUPTURE        = 26478,
    SPELL_HAMSTRING                   = 26211,
    SPELL_THRASH                      = 3391,
    SPELL_SUBMERGE_VISUAL             = 28819,
    SPELL_TELEPORT_GIANT_HOOK         = 26191,
    SPELL_TELEPORT_GIANT_HOOK_TRIGGER = 26205,
    SPELL_GROUND_TREMOR               = 6524,
    SPELL_DESPAWN_TENT_PORT           = 26404,

    SPELL_GROUND_RUPTURE            = 26139,
};

enum GiantClawActions
{
    GIANTCLAW_CHECKTARGET,
    GIANTCLAW_ACTION_MAX,
    GIANTCLAW_RUPTURE,
    GIANTCLAW_GROUND_TREMOR,
};

struct npc_giant_claw_tentacleAI : public CombatAI
{
    npc_giant_claw_tentacleAI(Creature* creature) : CombatAI(creature, GIANTCLAW_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCombatAction(GIANTCLAW_CHECKTARGET, 5000u);
        AddCustomAction(GIANTCLAW_RUPTURE, 2000u, [&]() { HandleRupture(); });
        AddCustomAction(GIANTCLAW_GROUND_TREMOR, 7000u, [&]() { HandleGroundTremor(); });
    }

    ScriptedInstance* m_instance;

    bool m_isLookingForMeleeTarget;

    void Reset() override
    {
        m_isLookingForMeleeTarget = false;

        m_creature->SetImmobilizedState(true);
    }

    void HandleRupture()
    {
        DoCastSpellIfCan(nullptr, SPELL_GIANT_GROUND_RUPTURE);
    }

    void HandleGroundTremor()
    {
        DoCastSpellIfCan(nullptr, SPELL_GROUND_TREMOR);
    }

    void JustDied(Unit* /*killer*/) override
    {
        m_creature->CastSpell(nullptr, SPELL_DESPAWN_TENT_PORT, TRIGGERED_OLD_TRIGGERED);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case GIANTCLAW_CHECKTARGET:
                // Check every second if we are tanked
                // If not, give 5 seconds to be tanked before teleporting to a target
                // If there is nobody in range, spawn a new tentacle at a new target location
                if (!m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, uint32(0), SELECT_FLAG_IN_MELEE_RANGE) && m_instance)
                {
                    if (m_isLookingForMeleeTarget)
                    {
                        m_creature->CastSpell(m_creature->GetVictim(), SPELL_TELEPORT_GIANT_HOOK_TRIGGER, TRIGGERED_OLD_TRIGGERED);
                        m_isLookingForMeleeTarget = false;
                        DisableCombatAction(action);
                    }
                    else
                    {
                        m_isLookingForMeleeTarget = true;
                        ResetCombatAction(action, 5 * IN_MILLISECONDS);
                    }
                }
                else
                    ResetCombatAction(action, 1 * IN_MILLISECONDS);
                break;
            default:
                break;
        }
    }
};

/*######
## at_stomach_cthun
######*/

bool AreaTrigger_at_stomach_cthun(Player* player, AreaTriggerEntry const* at)
{
    // Area trigger on the plateform at the end of C'Thun's stomach
    if (at->id == AREATRIGGER_STOMACH_1)
    {
        if (player->IsGameMaster() || !player->IsAlive())
            return false;

        // Summon the Exit Trigger NPC which will knockback the player outside the stomach
        if (Creature* trigger = GetClosestCreatureWithEntry(player, NPC_WORLD_TRIGGER, 20.0f))
            trigger->CastSpell(nullptr, SPELL_EXIT_STOMACH, TRIGGERED_OLD_TRIGGERED);

        return true;
    }

    // Area trigger at the ceiling of C'Thun's stomach (exit or entrance, depending on which way the "food" is going)
    if (at->id == AREATRIGGER_STOMACH_2)
    {
        if (player->IsGameMaster() || !player->IsAlive())
            return false;

        // Teleport player near the third area trigger
        player->NearTeleportTo(cthunLocations[3][0], cthunLocations[3][1], cthunLocations[3][2], cthunLocations[3][3]);
        return true;
    }

    // Area trigger located at C'Thun feet (foot?)
    if (at->id == AREATRIGGER_STOMACH_3)
    {
        // We need to check for the presence of Digestive Acid aura on player so the area trigger will only activate and knockback players coming from C'Thun's stomach
        // The knockback spell will remove the Digestive Acid aura
        if (player->IsGameMaster() || !player->IsAlive() || !player->HasAura(SPELL_DIGESTIVE_ACID))
            return false;

        // Check for the trigger NPC that will cast the knockback spell
        if (Creature* trigger = GetClosestCreatureWithEntry(player, NPC_WORLD_TRIGGER, 20.0f))
            trigger->CastSpell(player, SPELL_EXIT_STOMACH_EFFECT, TRIGGERED_OLD_TRIGGERED);

        return true;
    }
    return false;
}

// 26152, 26769 - Summon Eye Tentacles
struct PeriodicSummonEyeTrigger : public AuraScript
{
    void OnPeriodicTrigger(Aura* aura, PeriodicTriggerData& data) const override
    {
        if (Unit* caster = aura->GetCaster())
        {
            uint32 eyeTentaclesSpells[] = { SPELL_SUMMON_EYE_TENTACLE_1, SPELL_SUMMON_EYE_TENTACLE_2, SPELL_SUMMON_EYE_TENTACLE_3, SPELL_SUMMON_EYE_TENTACLE_4, SPELL_SUMMON_EYE_TENTACLE_5, SPELL_SUMMON_EYE_TENTACLE_6, SPELL_SUMMON_EYE_TENTACLE_7, SPELL_SUMMON_EYE_TENTACLE_8 };
            for (auto spellId:eyeTentaclesSpells)
                caster->CastSpell(nullptr, spellId, TRIGGERED_OLD_TRIGGERED);
        }
    }
};

// 26217, 26237, 26398, 26767 - Summon Giant Hook Tentacles, Summon Mouth Tentacles, Summon Hook Tentacles, Summon Giant Eye Tentacles
struct SummonCThunTentacles : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (Unit* target = spell->GetUnitTarget())
            {
                uint32 spellId;
                switch (spell->m_spellInfo->Id)
                {
                    case SPELL_SUMMON_MOUTH_TENTACLES_2:    spellId = SPELL_SUMMON_MOUTH_TENTACLE;      break;
                    case SPELL_GIANT_EYE_TENTACLES_2:       spellId = SPELL_SUMMON_GIANT_EYE_TENTACLE;  break;
                    case SPELL_SUMMON_HOOK_TENTACLES_2:     spellId = SPELL_SUMMON_HOOK_TENTACLE;       break;
                    case SPELL_SUMMON_GIANT_HOOKS_2:        spellId = SPELL_SUMMON_GIANT_HOOK_TENTACLE; break;
                    default: return;
                }
                target->CastSpell(nullptr, spellId, TRIGGERED_OLD_TRIGGERED);
            }
        }
    }
};

// 26137 - Rotate Trigger
struct RotateTrigger : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (Unit* target = spell->GetUnitTarget())
                target->CastSpell(nullptr, urand(0, 1) ? SPELL_ROTATE_360_LEFT : SPELL_ROTATE_360_RIGHT, TRIGGERED_OLD_TRIGGERED);
        }
    }
};

// 26009, 26136 - Rotate 360
struct PeriodicRotate : public AuraScript
{
    void OnPeriodicTrigger(Aura* aura, PeriodicTriggerData& data) const override
    {
        if (Unit* target = aura->GetTarget())
        {
            // Clear target before changing orientation
            target->SetTarget(nullptr);

            float newAngle = target->GetOrientation();
            if (aura->GetId() == SPELL_ROTATE_360_RIGHT)
                newAngle -= M_PI_F / 35;
            else
                newAngle += M_PI_F / 35;
            newAngle = MapManager::NormalizeOrientation(newAngle);
            target->SetFacingTo(newAngle);
            target->SetOrientation(newAngle);

            data.spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(SPELL_DARK_GLARE);
            data.caster = aura->GetCaster();
            data.target = nullptr;
        }
    }
};

// 26205 - Teleport Giant Hook Tentacle Trigger
struct HookTentacleTrigger : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (Unit* caster = spell->GetCaster())
            {
                // Caster will need to despawn and players cannot despawn
                if (caster->IsCreature())
                    return;

                if (Unit* target = spell->GetUnitTarget())
                {
                    caster->CastSpell(target, SPELL_TELEPORT_GIANT_HOOK, TRIGGERED_OLD_TRIGGERED);

                    // Self kill after spawning new tentacle
                    caster->AI()->SetCombatScriptStatus(true);
                    caster->SetTarget(nullptr);
                    caster->CastSpell(nullptr, SPELL_SUBMERGE_VISUAL, TRIGGERED_OLD_TRIGGERED);
                    static_cast<Creature*>(caster)->ForcedDespawn(1500);
                    if (Creature* portal = GetClosestCreatureWithEntry(caster, NPC_GIANT_TENTACLE_PORTAL, 5.0f))
                        portal->ForcedDespawn(1500);
                    return;
                }
            }
        }
    }
};

// 26230 - Exit Stomach
struct ExitStomach : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_1)
        {
            if (Unit* target = spell->GetUnitTarget())
            {
                // Remove Digestive Acid aura when player is ejected from C'Thun's stomach
                if (target->HasAura(SPELL_DIGESTIVE_ACID))
                    target->RemoveAurasDueToSpell(SPELL_DIGESTIVE_ACID);
                return;
            }
        }
    }
};

// 26256 - Check Reset
struct CheckReset : public SpellScript
{
    void OnSuccessfulFinish(Spell* spell) const override
    {
        // If we have at least one target, do nothing
        if (Unit* target = spell->GetUnitTarget())
            return;
        // Else: reset the encounter
        if (Unit* caster = spell->GetCaster())
            caster->CastSpell(nullptr, SPELL_RESET_ENCOUNTER, TRIGGERED_OLD_TRIGGERED);
    }
};

// 26332 - Summon Mouth Tentacle
struct CThunMouthTentacle : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_0)
        {
            if (Unit* target = aura->GetTarget())
            {
                if (!apply)
                    target->CastSpell(nullptr, SPELL_DIGESTIVE_ACID_TELEPORT, TRIGGERED_OLD_TRIGGERED);
            }
        }
    }
};

// 26476 - Digestive Acid
struct DigestiveAcidPeriodic : public AuraScript
{
    void OnPeriodicCalculateAmount(Aura* aura, uint32& /*amount*/) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_0)
        {
            if (Unit* target = aura->GetTarget())
            {
                // On every tick of the periodic damage, a new stack of Digestive Acid is applied
                if (Unit* caster = aura->GetCaster())
                    caster->CastSpell(target, SPELL_DIGESTIVE_ACID, TRIGGERED_OLD_TRIGGERED);
            }
        }
    }
};

// 26399 - Despawn Tentacles
struct DespawnTentacles : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        Unit* target = spell->GetUnitTarget();
        if (target->IsCreature())
            static_cast<Creature*>(target)->ForcedDespawn();
    }
};

// 26404 - Despawn Tent Port
struct DespawnTentPort : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (Unit* target = spell->GetUnitTarget())
            if (target->IsCreature())
                static_cast<Creature*>(target)->ForcedDespawn();
    }
};

// 26586 - Birth
struct BirthTentacles : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* caster = spell->GetCaster();
        if (!caster->IsCreature())
            return;
        switch (caster->GetEntry())
        {
            case NPC_EYE_TENTACLE:
            case NPC_CLAW_TENTACLE:
                caster->CastSpell(nullptr, SPELL_SUMMON_PORTAL, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
                caster->CastSpell(nullptr, SPELL_GROUND_RUPTURE, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
                break;
            case NPC_GIANT_CLAW_TENTACLE:
            case NPC_GIANT_EYE_TENTACLE:
                caster->CastSpell(nullptr, SPELL_SUMMON_GIANT_PORTAL, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
                break;
        }
    }
};

// 26140 - Summon Hook Tentacle
struct SummonHookTentacle : public SpellScript
{
    void OnSummon(Spell* /*spell*/, Creature* summon) const override
    {
        summon->CastSpell(nullptr, SPELL_BIRTH_TENTACLE, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
    }
};

// 26216 - Summon Giant Hook Tentacles
struct SummonGiantHookTentacles : public SpellScript
{
    void OnSummon(Spell* /*spell*/, Creature* summon) const override
    {
        summon->CastSpell(nullptr, SPELL_BIRTH_TENTACLE, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
    }
};

// 26150 - Summon Eye Tentacle
struct SummonEyeTentacle : public SpellScript
{
    void OnSummon(Spell* /*spell*/, Creature* summon) const override
    {
        summon->CastSpell(nullptr, SPELL_BIRTH_TENTACLE, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
    }
};

// 26768 - Summon Giant Eye Tentacles
struct SummonGiantEyeTentacles : public SpellScript
{
    void OnSummon(Spell* /*spell*/, Creature* summon) const override
    {
        summon->CastSpell(nullptr, SPELL_BIRTH_TENTACLE, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
    }
};

void AddSC_boss_cthun()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_eye_of_cthun";
    pNewScript->GetAI = &GetNewAIInstance<boss_eye_of_cthunAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_cthun";
    pNewScript->GetAI = &GetNewAIInstance<boss_cthunAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_giant_claw_tentacle";
    pNewScript->GetAI = &GetNewAIInstance<npc_giant_claw_tentacleAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "at_stomach_cthun";
    pNewScript->pAreaTrigger = &AreaTrigger_at_stomach_cthun;
    pNewScript->RegisterSelf();

    RegisterSpellScript<RotateTrigger>("spell_cthun_rotate_trigger");
    RegisterSpellScript<SummonCThunTentacles>("spell_cthun_tentacles_summon");
    RegisterSpellScript<HookTentacleTrigger>("spell_hook_tentacle_trigger");
    RegisterSpellScript<ExitStomach>("spell_cthun_exit_stomach");
    RegisterSpellScript<CheckReset>("spell_cthun_check_reset");
    RegisterSpellScript<PeriodicSummonEyeTrigger>("spell_cthun_periodic_eye_trigger");
    RegisterSpellScript<PeriodicRotate>("spell_cthun_periodic_rotate");
    RegisterSpellScript<CThunMouthTentacle>("spell_cthun_mouth_tentacle");
    RegisterSpellScript<DigestiveAcidPeriodic>("spell_cthun_digestive_acid_periodic");
    RegisterSpellScript<DespawnTentacles>("spell_despawn_tentacles");
    RegisterSpellScript<DespawnTentPort>("spell_despawn_tent_port");
    RegisterSpellScript<BirthTentacles>("spell_birth_tentacles");
    RegisterSpellScript<SummonHookTentacle>("spell_summon_hook_tentacle");
    RegisterSpellScript<SummonGiantHookTentacles>("spell_summon_giant_hook_tentacles");
    RegisterSpellScript<SummonEyeTentacle>("spell_summon_eye_tentacle");
    RegisterSpellScript<SummonGiantEyeTentacles>("spell_summon_giant_eye_tentacles");
}
