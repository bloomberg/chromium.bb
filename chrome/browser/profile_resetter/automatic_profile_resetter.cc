// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_delegate.h"
#include "chrome/browser/profile_resetter/jtl_interpreter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"


// AutomaticProfileResetter::EvaluationResults -------------------------------

// Encapsulates the output values extracted from the evaluator program.
struct AutomaticProfileResetter::EvaluationResults {
  EvaluationResults()
      : had_prompted_already(false),
        satisfied_criteria_mask(0),
        combined_status_mask(0) {}

  std::string memento_value_in_prefs;
  std::string memento_value_in_local_state;
  std::string memento_value_in_file;

  bool had_prompted_already;
  uint32 satisfied_criteria_mask;
  uint32 combined_status_mask;
};


// Helpers -------------------------------------------------------------------

namespace {

// Name constants for the field trial behind which we enable this feature.
const char kAutomaticProfileResetStudyName[] = "AutomaticProfileReset";
const char kAutomaticProfileResetStudyDryRunGroupName[] = "DryRun";
const char kAutomaticProfileResetStudyEnabledGroupName[] = "Enabled";
const char kAutomaticProfileResetStudyProgramParameterName[] = "program";
const char kAutomaticProfileResetStudyHashSeedParameterName[] = "hash_seed";

// How long to wait after start-up before unleashing the evaluation flow.
const int64 kEvaluationFlowDelayInSeconds = 55;

// Keys used in the input dictionary of the program.
const char kDefaultSearchProviderKey[] = "default_search_provider";
const char kDefaultSearchProviderIsUserControlledKey[] =
    "default_search_provider_iuc";
const char kLoadedModuleDigestsKey[] = "loaded_modules";
const char kLocalStateKey[] = "local_state";
const char kLocalStateIsUserControlledKey[] = "local_state_iuc";
const char kSearchProvidersKey[] = "search_providers";
const char kUserPreferencesKey[] = "preferences";
const char kUserPreferencesIsUserControlledKey[] = "preferences_iuc";

// Keys used in the output dictionary of the program.
const char kCombinedStatusMaskKeys[][26] = {
    "combined_status_mask_bit1", "combined_status_mask_bit2",
    "combined_status_mask_bit3", "combined_status_mask_bit4"};
const char kHadPromptedAlreadyKey[] = "had_prompted_already";
const char kSatisfiedCriteriaMaskKeys[][29] = {"satisfied_criteria_mask_bit1",
                                               "satisfied_criteria_mask_bit2"};

// Keys used in both the input and output dictionary of the program.
const char kMementoValueInFileKey[] = "memento_value_in_file";
const char kMementoValueInLocalStateKey[] = "memento_value_in_local_state";
const char kMementoValueInPrefsKey[] = "memento_value_in_prefs";

// Number of bits, and maximum value (exclusive) for the mask whose bits
// indicate which of reset criteria were satisfied.
const size_t kSatisfiedCriteriaMaskNumberOfBits = 2u;
const uint32 kSatisfiedCriteriaMaskMaximumValue =
    (1u << kSatisfiedCriteriaMaskNumberOfBits);

// Number of bits, and maximum value (exclusive) for the mask whose bits
// indicate if any of reset criteria were satisfied, and which of the mementos
// were already present.
const size_t kCombinedStatusMaskNumberOfBits = 4u;
const uint32 kCombinedStatusMaskMaximumValue =
    (1u << kCombinedStatusMaskNumberOfBits);

COMPILE_ASSERT(
    arraysize(kSatisfiedCriteriaMaskKeys) == kSatisfiedCriteriaMaskNumberOfBits,
    satisfied_criteria_mask_bits_mismatch);
COMPILE_ASSERT(
    arraysize(kCombinedStatusMaskKeys) == kCombinedStatusMaskNumberOfBits,
    combined_status_mask_bits_mismatch);

// Enumeration of the possible outcomes of showing the profile reset prompt.
enum PromptResult {
  // Prompt was not shown because only a dry-run was performed.
  PROMPT_NOT_SHOWN,
  PROMPT_ACTION_RESET,
  PROMPT_ACTION_NO_RESET,
  PROMPT_DISMISSED,
  // Prompt was still shown (not dismissed by the user) when Chrome was closed.
  PROMPT_IGNORED,
  PROMPT_RESULT_MAX
};

// Returns whether or not a dry-run shall be performed.
bool ShouldPerformDryRun() {
  return StartsWithASCII(
      base::FieldTrialList::FindFullName(kAutomaticProfileResetStudyName),
      kAutomaticProfileResetStudyDryRunGroupName, true);
}

// Returns whether or not a live-run shall be performed.
bool ShouldPerformLiveRun() {
  return StartsWithASCII(
      base::FieldTrialList::FindFullName(kAutomaticProfileResetStudyName),
      kAutomaticProfileResetStudyEnabledGroupName, true);
}

// Returns whether or not the currently active experiment group prescribes the
// program and hash seed to use instead of the baked-in ones.
bool DoesExperimentOverrideProgramAndHashSeed() {
#if defined(GOOGLE_CHROME_BUILD)
  std::map<std::string, std::string> params;
  chrome_variations::GetVariationParams(kAutomaticProfileResetStudyName,
                                        &params);
  return params.count(kAutomaticProfileResetStudyProgramParameterName) &&
         params.count(kAutomaticProfileResetStudyHashSeedParameterName);
#else
  return false;
#endif
}

// Deep-copies all preferences in |source| to a sub-tree named |value_tree_key|
// in |target_dictionary|, with path expansion, and also creates an isomorphic
// sub-tree under the key |is_user_controlled_tree_key| that contains only
// Boolean values, indicating whether or not the corresponding preferences are
// coming from the 'user' PrefStore.
void BuildSubTreesFromPreferences(const PrefService* source,
                                  const char* value_tree_key,
                                  const char* is_user_controlled_tree_key,
                                  base::DictionaryValue* target_dictionary) {
  scoped_ptr<base::DictionaryValue> pref_name_to_value_map(
      source->GetPreferenceValuesWithoutPathExpansion());
  std::vector<std::string> pref_names;
  pref_names.reserve(pref_name_to_value_map->size());
  for (base::DictionaryValue::Iterator it(*pref_name_to_value_map);
       !it.IsAtEnd(); it.Advance())
    pref_names.push_back(it.key());

  base::DictionaryValue* value_tree = new base::DictionaryValue;
  base::DictionaryValue* is_user_controlled_tree = new base::DictionaryValue;
  for (std::vector<std::string>::const_iterator it = pref_names.begin();
       it != pref_names.end(); ++it) {
    scoped_ptr<Value> pref_value_owned;
    if (pref_name_to_value_map->RemoveWithoutPathExpansion(*it,
                                                           &pref_value_owned)) {
      value_tree->Set(*it, pref_value_owned.release());
      const PrefService::Preference* pref = source->FindPreference(it->c_str());
      is_user_controlled_tree->Set(
          *it, new base::FundamentalValue(pref->IsUserControlled()));
    }
  }
  target_dictionary->Set(value_tree_key, value_tree);
  target_dictionary->Set(is_user_controlled_tree_key, is_user_controlled_tree);
}

}  // namespace


// AutomaticProfileResetter --------------------------------------------------

AutomaticProfileResetter::AutomaticProfileResetter(Profile* profile)
    : profile_(profile),
      state_(STATE_UNINITIALIZED),
      enumeration_of_loaded_modules_ready_(false),
      template_url_service_ready_(false),
      memento_in_prefs_(profile_),
      memento_in_local_state_(profile_),
      memento_in_file_(profile_),
      weak_ptr_factory_(this) {
  DCHECK(profile_);
}

AutomaticProfileResetter::~AutomaticProfileResetter() {}

void AutomaticProfileResetter::Initialize() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  if (!ShouldPerformDryRun() && !ShouldPerformLiveRun()) {
    state_ = STATE_DISABLED;
    return;
  }

  ui::ResourceBundle& resources(ui::ResourceBundle::GetSharedInstance());
  if (DoesExperimentOverrideProgramAndHashSeed()) {
    program_ = chrome_variations::GetVariationParamValue(
        kAutomaticProfileResetStudyName,
        kAutomaticProfileResetStudyProgramParameterName);
    hash_seed_ = chrome_variations::GetVariationParamValue(
        kAutomaticProfileResetStudyName,
        kAutomaticProfileResetStudyHashSeedParameterName);
  } else if (ShouldPerformLiveRun()) {
    program_ = resources.GetRawDataResource(
        IDR_AUTOMATIC_PROFILE_RESET_RULES).as_string();
    hash_seed_ = resources.GetRawDataResource(
        IDR_AUTOMATIC_PROFILE_RESET_HASH_SEED).as_string();
  } else {  // ShouldPerformDryRun()
    program_ = resources.GetRawDataResource(
        IDR_AUTOMATIC_PROFILE_RESET_RULES_DRY).as_string();
    hash_seed_ = resources.GetRawDataResource(
        IDR_AUTOMATIC_PROFILE_RESET_HASH_SEED_DRY).as_string();
  }

  delegate_.reset(new AutomaticProfileResetterDelegateImpl(
      TemplateURLServiceFactory::GetForProfile(profile_)));
  task_runner_for_waiting_ =
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI);

  state_ = STATE_INITIALIZED;
}

void AutomaticProfileResetter::Activate() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(state_ == STATE_INITIALIZED || state_ == STATE_DISABLED);

  if (state_ == STATE_INITIALIZED) {
    if (!program_.empty()) {
      // Some steps in the flow (e.g. loaded modules, file-based memento) are
      // IO-intensive, so defer execution until some time later.
      task_runner_for_waiting_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&AutomaticProfileResetter::PrepareEvaluationFlow,
                     weak_ptr_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kEvaluationFlowDelayInSeconds));
    } else {
      // Terminate early if there is no program included (nor set by tests).
      state_ = STATE_DISABLED;
    }
  }
}

void AutomaticProfileResetter::SetProgramForTesting(
    const std::string& program) {
  program_ = program;
}

void AutomaticProfileResetter::SetHashSeedForTesting(
    const std::string& hash_key) {
  hash_seed_ = hash_key;
}

void AutomaticProfileResetter::SetDelegateForTesting(
    scoped_ptr<AutomaticProfileResetterDelegate> delegate) {
  delegate_ = delegate.Pass();
}

void AutomaticProfileResetter::SetTaskRunnerForWaitingForTesting(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  task_runner_for_waiting_ = task_runner;
}

void AutomaticProfileResetter::PrepareEvaluationFlow() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_INITIALIZED);

  state_ = STATE_WAITING_ON_DEPENDENCIES;

  delegate_->RequestCallbackWhenTemplateURLServiceIsLoaded(
      base::Bind(&AutomaticProfileResetter::OnTemplateURLServiceIsLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
  delegate_->RequestCallbackWhenLoadedModulesAreEnumerated(
      base::Bind(&AutomaticProfileResetter::OnLoadedModulesAreEnumerated,
                 weak_ptr_factory_.GetWeakPtr()));
  delegate_->LoadTemplateURLServiceIfNeeded();
  delegate_->EnumerateLoadedModulesIfNeeded();
}

void AutomaticProfileResetter::OnTemplateURLServiceIsLoaded() {
  template_url_service_ready_ = true;
  OnDependencyIsReady();
}

void AutomaticProfileResetter::OnLoadedModulesAreEnumerated() {
  enumeration_of_loaded_modules_ready_ = true;
  OnDependencyIsReady();
}

void AutomaticProfileResetter::OnDependencyIsReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_WAITING_ON_DEPENDENCIES);

  if (template_url_service_ready_ && enumeration_of_loaded_modules_ready_) {
    state_ = STATE_READY;
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AutomaticProfileResetter::BeginEvaluationFlow,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void AutomaticProfileResetter::BeginEvaluationFlow() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_READY);
  DCHECK(!program_.empty());

  state_ = STATE_WORKING;
  memento_in_file_.ReadValue(
      base::Bind(&AutomaticProfileResetter::ContinueWithEvaluationFlow,
                 weak_ptr_factory_.GetWeakPtr()));
}

scoped_ptr<base::DictionaryValue>
    AutomaticProfileResetter::BuildEvaluatorProgramInput(
    const std::string& memento_value_in_file) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<base::DictionaryValue> input(new base::DictionaryValue);

  // Include memento values (or empty strings in case mementos are not there).
  input->SetString(kMementoValueInPrefsKey, memento_in_prefs_.ReadValue());
  input->SetString(kMementoValueInLocalStateKey,
                   memento_in_local_state_.ReadValue());
  input->SetString(kMementoValueInFileKey, memento_value_in_file);

  // Include all user (i.e. profile-specific) preferences, along with
  // information about whether the value is coming from the 'user' PrefStore.
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  BuildSubTreesFromPreferences(prefs,
                               kUserPreferencesKey,
                               kUserPreferencesIsUserControlledKey,
                               input.get());

  // Include all local state (i.e. shared) preferences, along with information
  // about whether the value is coming from the 'user' PrefStore.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  BuildSubTreesFromPreferences(
      local_state, kLocalStateKey, kLocalStateIsUserControlledKey, input.get());

  // Include all information related to search engines.
  scoped_ptr<base::DictionaryValue> default_search_provider_details(
      delegate_->GetDefaultSearchProviderDetails());
  input->Set(kDefaultSearchProviderKey,
             default_search_provider_details.release());

  scoped_ptr<base::ListValue> search_providers_details(
      delegate_->GetPrepopulatedSearchProvidersDetails());
  input->Set(kSearchProvidersKey, search_providers_details.release());

  input->SetBoolean(kDefaultSearchProviderIsUserControlledKey,
                    !delegate_->IsDefaultSearchProviderManaged());

  // Include information about loaded modules.
  scoped_ptr<base::ListValue> loaded_module_digests(
      delegate_->GetLoadedModuleNameDigests());
  input->Set(kLoadedModuleDigestsKey, loaded_module_digests.release());

  return input.Pass();
}

void AutomaticProfileResetter::ContinueWithEvaluationFlow(
    const std::string& memento_value_in_file) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_WORKING);
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  scoped_ptr<base::DictionaryValue> input(
      BuildEvaluatorProgramInput(memento_value_in_file));

  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::TaskRunner> task_runner =
      blocking_pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);

  base::PostTaskAndReplyWithResult(
      task_runner.get(),
      FROM_HERE,
      base::Bind(&EvaluateConditionsOnWorkerPoolThread,
                 hash_seed_,
                 program_,
                 base::Passed(&input)),
      base::Bind(&AutomaticProfileResetter::FinishEvaluationFlow,
                 weak_ptr_factory_.GetWeakPtr()));
}

// static
scoped_ptr<AutomaticProfileResetter::EvaluationResults>
    AutomaticProfileResetter::EvaluateConditionsOnWorkerPoolThread(
    const std::string& hash_seed,
    const std::string& program,
    scoped_ptr<base::DictionaryValue> program_input) {
  JtlInterpreter interpreter(hash_seed, program, program_input.get());
  interpreter.Execute();
  UMA_HISTOGRAM_ENUMERATION("AutomaticProfileReset.InterpreterResult",
                            interpreter.result(),
                            JtlInterpreter::RESULT_MAX);

  // In each case below, the respective field in result originally contains the
  // default, so if the getter fails, we still have the correct value there.
  scoped_ptr<EvaluationResults> results(new EvaluationResults);
  interpreter.GetOutputBoolean(kHadPromptedAlreadyKey,
                               &results->had_prompted_already);
  interpreter.GetOutputString(kMementoValueInPrefsKey,
                              &results->memento_value_in_prefs);
  interpreter.GetOutputString(kMementoValueInLocalStateKey,
                              &results->memento_value_in_local_state);
  interpreter.GetOutputString(kMementoValueInFileKey,
                              &results->memento_value_in_file);
  for (size_t i = 0; i < arraysize(kCombinedStatusMaskKeys); ++i) {
    bool flag = false;
    if (interpreter.GetOutputBoolean(kCombinedStatusMaskKeys[i], &flag) && flag)
      results->combined_status_mask |= (1 << i);
  }
  for (size_t i = 0; i < arraysize(kSatisfiedCriteriaMaskKeys); ++i) {
    bool flag = false;
    if (interpreter.GetOutputBoolean(kSatisfiedCriteriaMaskKeys[i], &flag) &&
        flag)
      results->satisfied_criteria_mask |= (1 << i);
  }
  return results.Pass();
}

void AutomaticProfileResetter::FinishEvaluationFlow(
    scoped_ptr<EvaluationResults> results) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_WORKING);

  ReportStatistics(results->satisfied_criteria_mask,
                   results->combined_status_mask);

  if (results->satisfied_criteria_mask != 0 && !results->had_prompted_already) {
    memento_in_prefs_.StoreValue(results->memento_value_in_prefs);
    memento_in_local_state_.StoreValue(results->memento_value_in_local_state);
    memento_in_file_.StoreValue(results->memento_value_in_file);

    if (ShouldPerformLiveRun()) {
      delegate_->ShowPrompt();
    } else {
      UMA_HISTOGRAM_ENUMERATION("AutomaticProfileReset.PromptResult",
                                PROMPT_NOT_SHOWN,
                                PROMPT_RESULT_MAX);
    }
  }

  state_ = STATE_DONE;
}

void AutomaticProfileResetter::ReportStatistics(uint32 satisfied_criteria_mask,
                                                uint32 combined_status_mask) {
  UMA_HISTOGRAM_ENUMERATION("AutomaticProfileReset.SatisfiedCriteriaMask",
                            satisfied_criteria_mask,
                            kSatisfiedCriteriaMaskMaximumValue);
  UMA_HISTOGRAM_ENUMERATION("AutomaticProfileReset.CombinedStatusMask",
                            combined_status_mask,
                            kCombinedStatusMaskMaximumValue);
}

void AutomaticProfileResetter::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  state_ = STATE_DISABLED;
  delegate_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}
