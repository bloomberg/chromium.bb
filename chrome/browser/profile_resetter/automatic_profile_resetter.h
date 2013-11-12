// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/task_runner.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_mementos.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class AutomaticProfileResetterDelegate;
class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

// This service is responsible for evaluating whether the criteria for showing
// the one-time profile reset prompt are satisfied, and for potentially
// triggering the prompt. To ensure that the prompt only appears at most once
// for any given profile, a "memento" that the prompt has appeared is written to
// the profile on disk; see automatic_profile_resetter_mementos.h for details.
// The service is created automatically with the Profile and is activated right
// away by its factory. To avoid delaying start-up, however, it will only start
// working after a short delay.
// All methods in this class shall be called on the UI thread, except when noted
// otherwise.
class AutomaticProfileResetter : public BrowserContextKeyedService {
 public:
  explicit AutomaticProfileResetter(Profile* profile);
  virtual ~AutomaticProfileResetter();

  // Initializes the service if it is enabled in the field trial. Otherwise,
  // skips the initialization steps, and also permanently disables the service.
  // Called by AutomaticProfileResetterFactory.
  void Initialize();

  // Fires up the service by unleashing the asynchronous evaluation flow, unless
  // the service has been already disabled in Initialize() or there is no
  // |program_| to run (in which case the service also gets disabled).
  // Called by the AutomaticProfileResetterFactory.
  void Activate();

  // Should be called before Activate().
  void SetProgramForTesting(const std::string& program);

  // Should be called before Activate().
  void SetHashSeedForTesting(const std::string& hash_seed);

  // Should be called before Activate().
  void SetDelegateForTesting(
      scoped_ptr<AutomaticProfileResetterDelegate> delegate);

  // Should be called before Activate(). Sets the task runner to be used to post
  // task |PrepareEvaluationFlow| in a delayed manner.
  void SetTaskRunnerForWaitingForTesting(
      const scoped_refptr<base::TaskRunner>& task_runner);

 private:
  class InputBuilder;
  struct EvaluationResults;

  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZED,
    STATE_DISABLED,
    STATE_WAITING_ON_DEPENDENCIES,
    STATE_READY,
    STATE_WORKING,
    STATE_DONE
  };

  // Prepares the asynchronous evaluation flow by requesting services that it
  // depends on to make themselves ready.
  void PrepareEvaluationFlow();

  // Called back by |resetter_delegate_| when the template URL service is ready.
  void OnTemplateURLServiceIsLoaded();

  // Called back by |resetter_delegate_| when the loaded modules have been
  // enumerated.
  void OnLoadedModulesAreEnumerated();

  // Invoked by the above two methods. Kicks off the actual evaluation flow.
  void OnDependencyIsReady();

  // Begins the asynchronous evaluation flow, which will assess whether the
  // criteria for showing the reset prompt are met, whether we have already
  // shown the prompt; and, in the end, will potentially trigger the prompt.
  void BeginEvaluationFlow();

  // Called by InputBuilder once it has finished assembling the |program_input|,
  // and will continue with the evaluation flow by triggering the evaluator
  // program on the worker thread.
  void ContinueWithEvaluationFlow(
      scoped_ptr<base::DictionaryValue> program_input);

  // Performs the bulk of the work. Invokes the JTL interpreter to run the
  // |program| that will evaluate whether the conditions are met for showing the
  // reset prompt. The program will make this decision based on the state
  // information contained in |input| in the form of key-value pairs. The
  // program will only see hashed keys and values that are produced using
  // |hash_seed| as a key.
  static scoped_ptr<EvaluationResults> EvaluateConditionsOnWorkerPoolThread(
      const std::string& hash_seed,
      const std::string& program,
      scoped_ptr<base::DictionaryValue> program_input);

  // Called back when EvaluateConditionsOnWorkerPoolThread() completes executing
  // the program with |results|. Finishes the evaluation flow, and, based on the
  // result, will potentially show the reset prompt.
  void FinishEvaluationFlow(scoped_ptr<EvaluationResults> results);

  // Reports the given metrics through UMA. Virtual, so it can be mocked out in
  // tests to verify that the correct value are being reported.
  virtual void ReportStatistics(uint32 satisfied_criteria_mask,
                                uint32 combined_status_mask);

  // BrowserContextKeyedService:
  virtual void Shutdown() OVERRIDE;

  Profile* profile_;

  State state_;
  bool enumeration_of_loaded_modules_ready_;
  bool template_url_service_ready_;

  scoped_ptr<InputBuilder> input_builder_;
  std::string hash_seed_;
  std::string program_;

  scoped_ptr<AutomaticProfileResetterDelegate> delegate_;
  scoped_refptr<base::TaskRunner> task_runner_for_waiting_;

  base::WeakPtrFactory<AutomaticProfileResetter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetter);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_H_
