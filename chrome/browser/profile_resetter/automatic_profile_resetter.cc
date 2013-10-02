// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/profile_resetter/jtl_interpreter.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Number of bits, and maximum value (exclusive) for the mask whose bits
// indicate which of reset criteria were satisfied.
const size_t kSatisfiedCriteriaMaskBits = 2;
const uint32 kSatisfiedCriteriaMaskMaximumValue =
    (1 << kSatisfiedCriteriaMaskBits);

// Number of bits, and maximum value (exclusive) for the mask whose bits
// indicate if any of reset criteria were satisfied, and which of the mementos
// were already present.
const size_t kCombinedStatusMaskBits = 4;
const uint32 kCombinedStatusMaskMaximumValue = (1 << kCombinedStatusMaskBits);

// Name constants for the field trial behind which we enable this feature.
const char kAutomaticProfileResetStudyName[] = "AutomaticProfileReset";
const char kAutomaticProfileResetStudyDryRunGroupName[] = "DryRun";
const char kAutomaticProfileResetStudyEnabledGroupName[] = "Enabled";

// Keys used in the input dictionary of the program.
// TODO(engedy): Add these here on an as-needed basis.

// Keys used in the output dictionary of the program.
const char kHadPromptedAlreadyKey[] = "had_prompted_already";
const char kSatisfiedCriteriaMaskKeys[][29] = {"satisfied_criteria_mask_bit1",
                                               "satisfied_criteria_mask_bit2"};
const char kCombinedStatusMaskKeys[][26] = {
    "combined_status_mask_bit1", "combined_status_mask_bit2",
    "combined_status_mask_bit3", "combined_status_mask_bit4"};

// Keys used in both the input and output dictionary of the program.
const char kMementoValueInPrefsKey[] = "memento_value_in_prefs";
const char kMementoValueInLocalStateKey[] = "memento_value_in_local_state";
const char kMementoValueInFileKey[] = "memento_value_in_file";

COMPILE_ASSERT(
    arraysize(kSatisfiedCriteriaMaskKeys) == kSatisfiedCriteriaMaskBits,
    satisfied_criteria_mask_bits_mismatch);
COMPILE_ASSERT(arraysize(kCombinedStatusMaskKeys) == kCombinedStatusMaskBits,
               combined_status_mask_bits_mismatch);

// Implementation detail classes ---------------------------------------------

class AutomaticProfileResetterDelegateImpl
    : public AutomaticProfileResetterDelegate {
 public:
  AutomaticProfileResetterDelegateImpl() {}
  virtual ~AutomaticProfileResetterDelegateImpl() {}

  // AutomaticProfileResetterDelegate overrides:

  virtual void ShowPrompt() OVERRIDE {
    // TODO(engedy): Call the UI from here once we have it.
  }

  virtual void ReportStatistics(uint32 satisfied_criteria_mask,
                                uint32 combined_status_mask) OVERRIDE {
    UMA_HISTOGRAM_ENUMERATION("AutomaticProfileReset.SatisfiedCriteriaMask",
                              satisfied_criteria_mask,
                              kSatisfiedCriteriaMaskMaximumValue);
    UMA_HISTOGRAM_ENUMERATION("AutomaticProfileReset.CombinedStatusMask",
                              combined_status_mask,
                              kCombinedStatusMaskMaximumValue);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterDelegateImpl);
};

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

}  // namespace

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

// AutomaticProfileResetter --------------------------------------------------

AutomaticProfileResetter::AutomaticProfileResetter(Profile* profile)
    : profile_(profile),
      state_(STATE_UNINITIALIZED),
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

  if (ShouldPerformDryRun() || ShouldPerformLiveRun()) {
    ui::ResourceBundle& resources(ui::ResourceBundle::GetSharedInstance());
    if (ShouldPerformLiveRun()) {
      program_ =
          resources.GetRawDataResource(IDR_AUTOMATIC_PROFILE_RESET_RULES);
      hash_seed_ =
          resources.GetRawDataResource(IDR_AUTOMATIC_PROFILE_RESET_HASH_SEED);
    } else {  // ShouldPerformDryRun()
      program_ =
          resources.GetRawDataResource(IDR_AUTOMATIC_PROFILE_RESET_RULES_DRY);
      hash_seed_ = resources.GetRawDataResource(
          IDR_AUTOMATIC_PROFILE_RESET_HASH_SEED_DRY);
    }
    delegate_.reset(new AutomaticProfileResetterDelegateImpl());

    state_ = STATE_READY;

    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AutomaticProfileResetter::BeginEvaluationFlow,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    state_ = STATE_DISABLED;
  }
}

bool AutomaticProfileResetter::ShouldPerformDryRun() const {
  return base::FieldTrialList::FindFullName(kAutomaticProfileResetStudyName) ==
         kAutomaticProfileResetStudyDryRunGroupName;
}

bool AutomaticProfileResetter::ShouldPerformLiveRun() const {
  return base::FieldTrialList::FindFullName(kAutomaticProfileResetStudyName) ==
         kAutomaticProfileResetStudyEnabledGroupName;
}

void AutomaticProfileResetter::BeginEvaluationFlow() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(state_, STATE_READY);

  if (!program_.empty()) {
    state_ = STATE_WORKING;
    memento_in_file_.ReadValue(
        base::Bind(&AutomaticProfileResetter::ContinueWithEvaluationFlow,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Terminate early if there is no program included (nor set by tests).
    state_ = STATE_DISABLED;
  }
}

scoped_ptr<base::DictionaryValue>
AutomaticProfileResetter::BuildEvaluatorProgramInput(
    const std::string& memento_value_in_file) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(engedy): Add any additional state here that is needed by the program.
  scoped_ptr<base::DictionaryValue> input(new base::DictionaryValue);
  input->SetString(kMementoValueInPrefsKey, memento_in_prefs_.ReadValue());
  input->SetString(kMementoValueInLocalStateKey,
                   memento_in_local_state_.ReadValue());
  input->SetString(kMementoValueInFileKey, memento_value_in_file);
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
    const base::StringPiece& hash_seed,
    const base::StringPiece& program,
    scoped_ptr<base::DictionaryValue> program_input) {
  std::string hash_seed_str(hash_seed.as_string());
  std::string program_str(program.as_string());
  JtlInterpreter interpreter(hash_seed_str, program_str, program_input.get());
  interpreter.Execute();
  UMA_HISTOGRAM_ENUMERATION("AutomaticProfileReset.InterpreterResult",
                            interpreter.result(),
                            JtlInterpreter::RESULT_MAX);

  // In each case below, the respective field in result originally contains the
  // default, so if the getter fails, we still have the correct value there.
  scoped_ptr<EvaluationResults> results(new EvaluationResults());
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

  delegate_->ReportStatistics(results->satisfied_criteria_mask,
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

void AutomaticProfileResetter::SetHashSeedForTesting(
    const base::StringPiece& hash_key) {
  hash_seed_ = hash_key;
}

void AutomaticProfileResetter::SetProgramForTesting(
    const base::StringPiece& program) {
  program_ = program;
}

void AutomaticProfileResetter::SetDelegateForTesting(
    AutomaticProfileResetterDelegate* delegate) {
  delegate_.reset(delegate);
}

void AutomaticProfileResetter::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  state_ = STATE_DISABLED;
  delegate_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}
