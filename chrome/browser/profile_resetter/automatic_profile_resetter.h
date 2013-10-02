// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_mementos.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class Profile;

// Defines the interface for the delegate that will actually show the prompt
// and/or report statistics on behalf of the AutomaticProfileResetter.
// The primary reason for this separation is to facilitate unit testing.
class AutomaticProfileResetterDelegate {
 public:
  virtual ~AutomaticProfileResetterDelegate() {}

  // Triggers showing the one-time profile settings reset prompt.
  virtual void ShowPrompt() = 0;

  // Reports the given metrics through UMA.
  virtual void ReportStatistics(uint32 satisfied_criteria_mask,
                                uint32 combined_status_mask) = 0;
};

// This service becomes busy shortly after start-up, and is responsible for
// evaluating if the criteria for showing the one-time profile reset prompt
// are satisfied, and will potentially trigger the prompt, but only a single
// time during the lifetime of a profile on disk (this is achieved by storing
// "mementos"). All methods in this class shall be called on the UI thread.
class AutomaticProfileResetter : public BrowserContextKeyedService {
 public:
  explicit AutomaticProfileResetter(Profile* profile);
  virtual ~AutomaticProfileResetter();

  // Initializes the service, and sets up the asynchronous evaluation flow.
  // Called by AutomaticProfileResetterFactory.
  void Initialize();

  // Should be called after Initialize().
  void SetHashSeedForTesting(const base::StringPiece& hash_seed);

  // Should be called after Initialize().
  void SetProgramForTesting(const base::StringPiece& program);

  // Should be called after Initialize(). Takes ownership.
  void SetDelegateForTesting(AutomaticProfileResetterDelegate* delegate);

 private:
  struct EvaluationResults;

  enum State {
    STATE_UNINITIALIZED,
    STATE_DISABLED,
    STATE_READY,
    STATE_WORKING,
    STATE_DONE
  };

  // Returns whether or not a dry-run shall be performed.
  bool ShouldPerformDryRun() const;

  // Returns whether or not a live-run shall be performed.
  bool ShouldPerformLiveRun() const;

  // Begins the asynchronous evaluation flow, which will assess whether the
  // criteria for showing the reset prompt are met, whether we have already
  // shown the prompt, and, in the end, will potentially trigger the prompt.
  void BeginEvaluationFlow();

  // Called back by |memento_in_file_| once it has finished reading the value of
  // the file-based memento. Continues the evaluation flow with collecting state
  // information and assembling it as the input for the evaluator program.
  void ContinueWithEvaluationFlow(const std::string& memento_value_in_file);

  // Prepare the input of the evaluator program. This will contain all the state
  // information required to assess whether or not the conditions for showing
  // the reset prompt are met.
  scoped_ptr<base::DictionaryValue> BuildEvaluatorProgramInput(
      const std::string& memento_value_in_file);

  // Performs the bulk of the work. Invokes the interpreter to run the |program|
  // that will evaluate whether the conditions are met for showing the reset
  // prompt. The program will make this decision based on the state information
  // contained in |input| in the form of key-value pairs. The program will only
  // see hashed keys and values that are produced using |hash_seed| as a key.
  static scoped_ptr<EvaluationResults> EvaluateConditionsOnWorkerPoolThread(
      const base::StringPiece& hash_seed,
      const base::StringPiece& program,
      scoped_ptr<base::DictionaryValue> program_input);

  // Called back when EvaluateConditionsOnWorkerPoolThread completes executing
  // the program with |results|. Finishes the evaluation flow, and, based on the
  // result, will potentially show the reset prompt.
  void FinishEvaluationFlow(scoped_ptr<EvaluationResults> results);

  // BrowserContextKeyedService overrides:
  virtual void Shutdown() OVERRIDE;

  Profile* profile_;

  State state_;

  base::StringPiece hash_seed_;
  base::StringPiece program_;

  PreferenceHostedPromptMemento memento_in_prefs_;
  LocalStateHostedPromptMemento memento_in_local_state_;
  FileHostedPromptMemento memento_in_file_;

  scoped_ptr<AutomaticProfileResetterDelegate> delegate_;

  base::WeakPtrFactory<AutomaticProfileResetter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetter);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_H_
