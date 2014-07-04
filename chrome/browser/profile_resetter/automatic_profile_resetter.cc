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
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_delegate.h"
#include "chrome/browser/profile_resetter/jtl_interpreter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"


// Helpers -------------------------------------------------------------------

namespace {

// Name constants for the field trial behind which we enable this feature.
const char kAutomaticProfileResetStudyName[] = "AutomaticProfileReset";
const char kAutomaticProfileResetStudyDryRunGroupName[] = "DryRun";
const char kAutomaticProfileResetStudyEnabledGroupName[] = "Enabled";
#if defined(GOOGLE_CHROME_BUILD)
const char kAutomaticProfileResetStudyProgramParameterName[] = "program";
const char kAutomaticProfileResetStudyHashSeedParameterName[] = "hash_seed";
#endif

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
const char kCombinedStatusMaskKeyPrefix[] = "combined_status_mask_bit";
const char kHadPromptedAlreadyKey[] = "had_prompted_already";
const char kShouldPromptKey[] = "should_prompt";
const char kSatisfiedCriteriaMaskKeyPrefix[] = "satisfied_criteria_mask_bit";

// Keys used in both the input and output dictionary of the program.
const char kMementoValueInFileKey[] = "memento_value_in_file";
const char kMementoValueInLocalStateKey[] = "memento_value_in_local_state";
const char kMementoValueInPrefsKey[] = "memento_value_in_prefs";

// Number of bits, and maximum value (exclusive) for the mask whose bits
// indicate which of reset criteria were satisfied.
const size_t kSatisfiedCriteriaMaskNumberOfBits = 5u;
const uint32 kSatisfiedCriteriaMaskMaximumValue =
    (1u << kSatisfiedCriteriaMaskNumberOfBits);

// Number of bits, and maximum value (exclusive) for the mask whose bits
// indicate if any of reset criteria were satisfied, and which of the mementos
// were already present.
const size_t kCombinedStatusMaskNumberOfBits = 4u;
const uint32 kCombinedStatusMaskMaximumValue =
    (1u << kCombinedStatusMaskNumberOfBits);

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

// If the currently active experiment group prescribes a |program| and
// |hash_seed| to use instead of the baked-in ones, retrieves those and returns
// true. Otherwise, returns false.
bool GetProgramAndHashSeedOverridesFromExperiment(std::string* program,
                                                  std::string* hash_seed) {
  DCHECK(program);
  DCHECK(hash_seed);
#if defined(GOOGLE_CHROME_BUILD)
  std::map<std::string, std::string> params;
  chrome_variations::GetVariationParams(kAutomaticProfileResetStudyName,
                                        &params);
  if (params.count(kAutomaticProfileResetStudyProgramParameterName) &&
      params.count(kAutomaticProfileResetStudyHashSeedParameterName)) {
    program->swap(params[kAutomaticProfileResetStudyProgramParameterName]);
    hash_seed->swap(params[kAutomaticProfileResetStudyHashSeedParameterName]);
    return true;
  }
#endif
  return false;
}

// Takes |pref_name_to_value_map|, which shall be a deep-copy of all preferences
// in |source| without path expansion; and (1.) creates a sub-tree from it named
// |value_tree_key| in |target_dictionary| with path expansion, and (2.) also
// creates an isomorphic sub-tree under the key |is_user_controlled_tree_key|
// that contains only Boolean values indicating whether or not the corresponding
// preference is coming from the 'user' PrefStore.
void BuildSubTreesFromPreferences(
    scoped_ptr<base::DictionaryValue> pref_name_to_value_map,
    const PrefService* source,
    const char* value_tree_key,
    const char* is_user_controlled_tree_key,
    base::DictionaryValue* target_dictionary) {
  std::vector<std::string> pref_names;
  pref_names.reserve(pref_name_to_value_map->size());
  for (base::DictionaryValue::Iterator it(*pref_name_to_value_map);
       !it.IsAtEnd(); it.Advance())
    pref_names.push_back(it.key());

  base::DictionaryValue* value_tree = new base::DictionaryValue;
  base::DictionaryValue* is_user_controlled_tree = new base::DictionaryValue;
  for (std::vector<std::string>::const_iterator it = pref_names.begin();
       it != pref_names.end(); ++it) {
    scoped_ptr<base::Value> pref_value_owned;
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


// AutomaticProfileResetter::InputBuilder ------------------------------------

// Collects all the information that is required by the evaluator program to
// assess whether or not the conditions for showing the reset prompt are met.
//
// This necessitates a lot of work that has to be performed on the UI thread,
// such as: accessing the Preferences, Local State, and TemplateURLService.
// In order to keep the browser responsive, the UI thread shall not be blocked
// for long consecutive periods of time. Unfortunately, we cannot reduce the
// total amount of work. Instead, what this class does is to split the work into
// shorter tasks that are posted one-at-a-time to the UI thread in a serial
// fashion, so as to give a chance to run other tasks that have accumulated in
// the meantime.
class AutomaticProfileResetter::InputBuilder
    : public base::SupportsWeakPtr<InputBuilder> {
 public:
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
      ProgramInputCallback;

  // The dependencies must have been initialized through |delegate|, i.e. the
  // RequestCallback[...] methods must have already fired before calling this.
  InputBuilder(Profile* profile, AutomaticProfileResetterDelegate* delegate)
      : profile_(profile),
        delegate_(delegate),
        memento_in_prefs_(profile_),
        memento_in_local_state_(profile_),
        memento_in_file_(profile_) {}
  ~InputBuilder() {}

  // Assembles the data required by the evaluator program into a dictionary
  // format, and posts it back to the UI thread with |callback| once ready. In
  // order not to block the UI thread for long consecutive periods of time, the
  // work is divided into smaller tasks, see class comment above for details.
  // It is safe to destroy |this| immediately from within the |callback|.
  void BuildEvaluatorProgramInput(const ProgramInputCallback& callback) {
    DCHECK(!data_);
    DCHECK(!callback.is_null());
    data_.reset(new base::DictionaryValue);
    callback_ = callback;

    AddAsyncTask(base::Bind(&InputBuilder::IncludeMementoValues, AsWeakPtr()));
    AddTask(base::Bind(&InputBuilder::IncludeUserPreferences, AsWeakPtr()));
    AddTask(base::Bind(&InputBuilder::IncludeLocalState, AsWeakPtr()));
    AddTask(base::Bind(&InputBuilder::IncludeSearchEngines, AsWeakPtr()));
    AddTask(base::Bind(&InputBuilder::IncludeLoadedModules, AsWeakPtr()));

    // Each task will post the next one. Just trigger the chain reaction.
    PostNextTask();
  }

 private:
  // Asynchronous task that includes memento values (or empty strings in case
  // mementos are not there).
  void IncludeMementoValues() {
    data_->SetString(kMementoValueInPrefsKey, memento_in_prefs_.ReadValue());
    data_->SetString(kMementoValueInLocalStateKey,
                     memento_in_local_state_.ReadValue());
    memento_in_file_.ReadValue(base::Bind(
        &InputBuilder::IncludeFileBasedMementoCallback, AsWeakPtr()));
  }

  // Called back by |memento_in_file_| once the |memento_value| has been read.
  void IncludeFileBasedMementoCallback(const std::string& memento_value) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    data_->SetString(kMementoValueInFileKey, memento_value);
    // As an asynchronous task, we need to take care of posting the next task.
    PostNextTask();
  }

  // Task that includes all user (i.e. profile-specific) preferences, along with
  // information about whether the value is coming from the 'user' PrefStore.
  // This is the most expensive operation, so it is itself split into two parts.
  void IncludeUserPreferences() {
    PrefService* prefs = profile_->GetPrefs();
    DCHECK(prefs);
    scoped_ptr<base::DictionaryValue> pref_name_to_value_map(
        prefs->GetPreferenceValuesWithoutPathExpansion());
    AddTask(base::Bind(&InputBuilder::IncludeUserPreferencesPartTwo,
                       AsWeakPtr(),
                       base::Passed(&pref_name_to_value_map)));
  }

  // Second part to above.
  void IncludeUserPreferencesPartTwo(
      scoped_ptr<base::DictionaryValue> pref_name_to_value_map) {
    PrefService* prefs = profile_->GetPrefs();
    DCHECK(prefs);
    BuildSubTreesFromPreferences(
        pref_name_to_value_map.Pass(),
        prefs,
        kUserPreferencesKey,
        kUserPreferencesIsUserControlledKey,
        data_.get());
  }

  // Task that includes all local state (i.e. shared) preferences, along with
  // information about whether the value is coming from the 'user' PrefStore.
  void IncludeLocalState() {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    scoped_ptr<base::DictionaryValue> pref_name_to_value_map(
        local_state->GetPreferenceValuesWithoutPathExpansion());
    BuildSubTreesFromPreferences(
        pref_name_to_value_map.Pass(),
        local_state,
        kLocalStateKey,
        kLocalStateIsUserControlledKey,
        data_.get());
  }

  // Task that includes all information related to search engines.
  void IncludeSearchEngines() {
    scoped_ptr<base::DictionaryValue> default_search_provider_details(
        delegate_->GetDefaultSearchProviderDetails());
    data_->Set(kDefaultSearchProviderKey,
               default_search_provider_details.release());

    scoped_ptr<base::ListValue> search_providers_details(
        delegate_->GetPrepopulatedSearchProvidersDetails());
    data_->Set(kSearchProvidersKey, search_providers_details.release());

    data_->SetBoolean(kDefaultSearchProviderIsUserControlledKey,
                      !delegate_->IsDefaultSearchProviderManaged());
  }

  // Task that includes information about loaded modules.
  void IncludeLoadedModules() {
    scoped_ptr<base::ListValue> loaded_module_digests(
        delegate_->GetLoadedModuleNameDigests());
    data_->Set(kLoadedModuleDigestsKey, loaded_module_digests.release());
  }

  // -------------------------------------------------------------------------

  // Adds a |task| that can do as much asynchronous processing as it wants, but
  // will need to finally call PostNextTask() on the UI thread when done.
  void AddAsyncTask(const base::Closure& task) {
    task_queue_.push(task);
  }

  // Convenience wrapper for synchronous tasks.
  void SynchronousTaskWrapper(const base::Closure& task) {
    base::ElapsedTimer timer;
    task.Run();
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "AutomaticProfileReset.InputBuilder.TaskDuration",
        timer.Elapsed(),
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromSeconds(2),
        50);
    PostNextTask();
  }

  // Adds a task that needs to finish synchronously. In exchange, PostNextTask()
  // is called automatically when the |task| returns, and execution time is
  // measured.
  void AddTask(const base::Closure& task) {
    task_queue_.push(
        base::Bind(&InputBuilder::SynchronousTaskWrapper, AsWeakPtr(), task));
  }

  // Posts the next task from the |task_queue_|, unless it is exhausted, in
  // which case it posts |callback_| to return with the results.
  void PostNextTask() {
    base::Closure next_task;
    if (task_queue_.empty()) {
      next_task = base::Bind(callback_, base::Passed(&data_));
    } else {
      next_task = task_queue_.front();
      task_queue_.pop();
    }
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE, next_task);
  }

  Profile* profile_;
  AutomaticProfileResetterDelegate* delegate_;
  ProgramInputCallback callback_;

  PreferenceHostedPromptMemento memento_in_prefs_;
  LocalStateHostedPromptMemento memento_in_local_state_;
  FileHostedPromptMemento memento_in_file_;

  scoped_ptr<base::DictionaryValue> data_;
  std::queue<base::Closure> task_queue_;

  DISALLOW_COPY_AND_ASSIGN(InputBuilder);
};


// AutomaticProfileResetter::EvaluationResults -------------------------------

// Encapsulates the output values extracted from the evaluator program.
struct AutomaticProfileResetter::EvaluationResults {
  EvaluationResults()
      : should_prompt(false),
        had_prompted_already(false),
        satisfied_criteria_mask(0),
        combined_status_mask(0) {}

  std::string memento_value_in_prefs;
  std::string memento_value_in_local_state;
  std::string memento_value_in_file;

  bool should_prompt;
  bool had_prompted_already;
  uint32 satisfied_criteria_mask;
  uint32 combined_status_mask;
};


// AutomaticProfileResetter --------------------------------------------------

AutomaticProfileResetter::AutomaticProfileResetter(Profile* profile)
    : profile_(profile),
      state_(STATE_UNINITIALIZED),
      enumeration_of_loaded_modules_ready_(false),
      template_url_service_ready_(false),
      has_already_dismissed_prompt_(false),
      should_show_reset_banner_(false),
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

  if (!GetProgramAndHashSeedOverridesFromExperiment(&program_, &hash_seed_)) {
    ui::ResourceBundle& resources(ui::ResourceBundle::GetSharedInstance());
    if (ShouldPerformLiveRun()) {
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
  }

  delegate_.reset(new AutomaticProfileResetterDelegateImpl(
      profile_, ProfileResetter::ALL));
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

void AutomaticProfileResetter::TriggerProfileReset(bool send_feedback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_HAS_SHOWN_BUBBLE);

  state_ = STATE_PERFORMING_RESET;
  should_show_reset_banner_ = false;

  ReportPromptResult(PROMPT_ACTION_RESET);
  delegate_->TriggerProfileSettingsReset(
      send_feedback,
      base::Bind(&AutomaticProfileResetter::OnProfileSettingsResetCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AutomaticProfileResetter::SkipProfileReset() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_HAS_SHOWN_BUBBLE);

  should_show_reset_banner_ = false;

  ReportPromptResult(PROMPT_ACTION_NO_RESET);
  delegate_->DismissPrompt();
  FinishResetPromptFlow();
}

bool AutomaticProfileResetter::IsResetPromptFlowActive() const {
  return state_ == STATE_HAS_TRIGGERED_PROMPT ||
      state_ == STATE_HAS_SHOWN_BUBBLE;
}

bool AutomaticProfileResetter::ShouldShowResetBanner() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return should_show_reset_banner_ && ShouldPerformLiveRun();
}

void AutomaticProfileResetter::NotifyDidShowResetBubble() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_HAS_TRIGGERED_PROMPT);

  state_ = STATE_HAS_SHOWN_BUBBLE;

  PersistMementos();
  ReportPromptResult(PROMPT_SHOWN_BUBBLE);
}

void AutomaticProfileResetter::NotifyDidOpenWebUIResetDialog() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // This notification is invoked unconditionally by the WebUI, only care about
  // it when the prompt flow is currently active (and not yet resetting).
  if (state_ == STATE_HAS_TRIGGERED_PROMPT ||
      state_ == STATE_HAS_SHOWN_BUBBLE) {
    has_already_dismissed_prompt_ = true;
    delegate_->DismissPrompt();
  }
}

void AutomaticProfileResetter::NotifyDidCloseWebUIResetDialog(
    bool performed_reset) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // This notification is invoked unconditionally by the WebUI, only care about
  // it when the prompt flow is currently active (and not yet resetting).
  if (state_ == STATE_HAS_TRIGGERED_PROMPT ||
      state_ == STATE_HAS_SHOWN_BUBBLE) {
    if (!has_already_dismissed_prompt_)
      delegate_->DismissPrompt();
    if (state_ == STATE_HAS_TRIGGERED_PROMPT) {
      PersistMementos();
      ReportPromptResult(performed_reset ?
          PROMPT_NOT_SHOWN_BUBBLE_BUT_HAD_WEBUI_RESET :
          PROMPT_NOT_SHOWN_BUBBLE_BUT_HAD_WEBUI_NO_RESET);
    } else {  // if (state_ == STATE_HAS_SHOWN_PROMPT)
      ReportPromptResult(performed_reset ?
          PROMPT_FOLLOWED_BY_WEBUI_RESET :
          PROMPT_FOLLOWED_BY_WEBUI_NO_RESET);
    }
    FinishResetPromptFlow();
  }
}

void AutomaticProfileResetter::NotifyDidCloseWebUIResetBanner() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  should_show_reset_banner_ = false;
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

void AutomaticProfileResetter::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // We better not do anything substantial at this point. The metrics service
  // has already been shut down; and local state has already been commited to
  // file (in the regular fashion) for the last time.

  state_ = STATE_DISABLED;

  weak_ptr_factory_.InvalidateWeakPtrs();
  delegate_.reset();
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
  DCHECK(!input_builder_);

  state_ = STATE_EVALUATING_CONDITIONS;

  input_builder_.reset(new InputBuilder(profile_, delegate_.get()));
  input_builder_->BuildEvaluatorProgramInput(
      base::Bind(&AutomaticProfileResetter::ContinueWithEvaluationFlow,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AutomaticProfileResetter::ContinueWithEvaluationFlow(
    scoped_ptr<base::DictionaryValue> program_input) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_EVALUATING_CONDITIONS);

  input_builder_.reset();

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
                 base::Passed(&program_input)),
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
  UMA_HISTOGRAM_SPARSE_SLOWLY("AutomaticProfileReset.ProgramChecksum",
                              interpreter.CalculateProgramChecksum());

  // In each case below, the respective field in result originally contains the
  // default, so if the getter fails, we still have the correct value there.
  scoped_ptr<EvaluationResults> results(new EvaluationResults);
  interpreter.GetOutputBoolean(kShouldPromptKey, &results->should_prompt);
  interpreter.GetOutputBoolean(kHadPromptedAlreadyKey,
                               &results->had_prompted_already);
  interpreter.GetOutputString(kMementoValueInPrefsKey,
                              &results->memento_value_in_prefs);
  interpreter.GetOutputString(kMementoValueInLocalStateKey,
                              &results->memento_value_in_local_state);
  interpreter.GetOutputString(kMementoValueInFileKey,
                              &results->memento_value_in_file);
  for (size_t i = 0; i < kCombinedStatusMaskNumberOfBits; ++i) {
    bool flag = false;
    std::string mask_i_th_bit_key =
        kCombinedStatusMaskKeyPrefix + base::IntToString(i + 1);
    if (interpreter.GetOutputBoolean(mask_i_th_bit_key, &flag) && flag)
      results->combined_status_mask |= (1 << i);
  }
  for (size_t i = 0; i < kSatisfiedCriteriaMaskNumberOfBits; ++i) {
    bool flag = false;
    std::string mask_i_th_bit_key =
        kSatisfiedCriteriaMaskKeyPrefix + base::IntToString(i + 1);
    if (interpreter.GetOutputBoolean(mask_i_th_bit_key, &flag) && flag)
      results->satisfied_criteria_mask |= (1 << i);
  }
  return results.Pass();
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

void AutomaticProfileResetter::FinishEvaluationFlow(
    scoped_ptr<EvaluationResults> results) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_EVALUATING_CONDITIONS);

  ReportStatistics(results->satisfied_criteria_mask,
                   results->combined_status_mask);

  if (results->should_prompt)
    should_show_reset_banner_ = true;

  if (results->should_prompt && !results->had_prompted_already) {
    evaluation_results_ = results.Pass();
    BeginResetPromptFlow();
  } else {
    state_ = STATE_DONE;
  }
}

void AutomaticProfileResetter::BeginResetPromptFlow() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_EVALUATING_CONDITIONS);

  state_ = STATE_HAS_TRIGGERED_PROMPT;

  if (ShouldPerformLiveRun() && delegate_->TriggerPrompt()) {
    // Start fetching the brandcoded default settings speculatively in the
    // background, so as to reduce waiting time if the user chooses to go
    // through with the reset.
    delegate_->FetchBrandcodedDefaultSettingsIfNeeded();
  } else {
    PersistMementos();
    ReportPromptResult(PROMPT_NOT_TRIGGERED);
    FinishResetPromptFlow();
  }
}

void AutomaticProfileResetter::OnProfileSettingsResetCompleted() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_PERFORMING_RESET);

  delegate_->DismissPrompt();
  FinishResetPromptFlow();
}

void AutomaticProfileResetter::ReportPromptResult(PromptResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "AutomaticProfileReset.PromptResult", result, PROMPT_RESULT_MAX);
}

void AutomaticProfileResetter::PersistMementos() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(state_ == STATE_HAS_TRIGGERED_PROMPT ||
         state_ == STATE_HAS_SHOWN_BUBBLE);
  DCHECK(evaluation_results_);

  PreferenceHostedPromptMemento memento_in_prefs(profile_);
  LocalStateHostedPromptMemento memento_in_local_state(profile_);
  FileHostedPromptMemento memento_in_file(profile_);

  memento_in_prefs.StoreValue(evaluation_results_->memento_value_in_prefs);
  memento_in_local_state.StoreValue(
      evaluation_results_->memento_value_in_local_state);
  memento_in_file.StoreValue(evaluation_results_->memento_value_in_file);

  evaluation_results_.reset();
}

void AutomaticProfileResetter::FinishResetPromptFlow() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(state_ == STATE_HAS_TRIGGERED_PROMPT ||
         state_ == STATE_HAS_SHOWN_BUBBLE ||
         state_ == STATE_PERFORMING_RESET);
  DCHECK(!evaluation_results_);

  state_ = STATE_DONE;
}
