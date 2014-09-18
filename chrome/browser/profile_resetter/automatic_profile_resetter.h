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
#include "components/keyed_service/core/keyed_service.h"

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
class AutomaticProfileResetter : public KeyedService {
 public:
  // Enumeration listing the possible outcomes of triggering the profile reset
  // prompt.
  enum PromptResult {
    // The reset prompt was not triggered because only a dry-run was performed,
    // or because it was not supported on the current platform.
    PROMPT_NOT_TRIGGERED,
    // The reset bubble actually got shown. In contrast to the wrench menu item
    // that can always be shown, the bubble might be delayed or might never be
    // shown if another bubble was shown at the time of triggering the prompt.
    // This enumeration value is usually recorded in conjunction with another
    // PromptResult, the absence of which indicates that the prompt was ignored.
    PROMPT_SHOWN_BUBBLE,
    // The user selected "Reset" or "No, thanks" (respectively) directly from
    // within the bubble.
    PROMPT_ACTION_RESET,
    PROMPT_ACTION_NO_RESET,
    // The reset bubble was shown, then dismissed without taking definitive
    // action. Then, however, the user initiated or refrained from doing a reset
    // (respectively) from the conventional, WebUI-based reset dialog.
    PROMPT_FOLLOWED_BY_WEBUI_RESET,
    PROMPT_FOLLOWED_BY_WEBUI_NO_RESET,
    // The reset bubble was suppressed (not shown) because another bubble was
    // already being shown at the time. Regardless, however, the user initiated
    // or refrained from doing a reset (respectively) from the conventional,
    // WebUI-based reset dialog.
    PROMPT_NOT_SHOWN_BUBBLE_BUT_HAD_WEBUI_RESET,
    PROMPT_NOT_SHOWN_BUBBLE_BUT_HAD_WEBUI_NO_RESET,
    PROMPT_RESULT_MAX
  };

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

  // Called in case the user chooses to reset their profile settings from inside
  // the reset bubble. Will trigger the reset, optionally |send_feedback|, and
  // conclude the reset prompt flow.
  void TriggerProfileReset(bool send_feedback);

  // Called in case the user chooses from inside the reset bubble that they do
  // not want to reset their profile settings. Will conclude the reset prompt
  // flow without setting off a reset.
  void SkipProfileReset();

  // Returns whether or not the profile reset prompt flow is currently active,
  // that is, we have triggered the prompt and are waiting for the user to take
  // definitive action (and we are not yet performing a reset).
  bool IsResetPromptFlowActive() const;

  // Returns whether or not the profile reset banner should be shown on the
  // WebUI-based settings page.
  bool ShouldShowResetBanner() const;

  // Called to give notice that the reset bubble has actually been shown.
  void NotifyDidShowResetBubble();

  // Called to give notice that the conventional, WebUI-based settings reset
  // dialog has been opened. This will dismiss the menu item in the wrench menu.
  // This should always be followed by a corresponding call to
  // NotifyDidCloseWebUIResetDialog().
  void NotifyDidOpenWebUIResetDialog();

  // Called to give notice that the conventional, WebUI-based settings reset
  // dialog has been closed, with |performed_reset| indicating whether or not a
  // reset was requested. This is required so that we can record the appropriate
  // PromptResult, dismiss the prompt, and conclude the reset prompt flow early
  // without setting off any resets in the future.
  void NotifyDidCloseWebUIResetDialog(bool performed_reset);

  // Called to give notice that reset banner has been dismissed as a result of
  // user action on the WebUI-based settings page itself.
  void NotifyDidCloseWebUIResetBanner();

  base::WeakPtr<AutomaticProfileResetter> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

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

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

 private:
  class InputBuilder;
  struct EvaluationResults;

  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZED,
    STATE_DISABLED,
    STATE_WAITING_ON_DEPENDENCIES,
    STATE_READY,
    STATE_EVALUATING_CONDITIONS,
    // The reset prompt has been triggered; but the reset bubble has not yet
    // been shown.
    STATE_HAS_TRIGGERED_PROMPT,
    // The reset prompt has been triggered; the reset bubble has been shown, and
    // potentially already dismissed by the user.
    STATE_HAS_SHOWN_BUBBLE,
    STATE_PERFORMING_RESET,
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

  // Reports the given metrics through UMA. Virtual, so it can be mocked out in
  // tests to verify that the correct value are being reported.
  virtual void ReportStatistics(uint32 satisfied_criteria_mask,
                                uint32 combined_status_mask);

  // Called back when EvaluateConditionsOnWorkerPoolThread completes executing
  // the program with |results|. Finishes the evaluation flow, and, based on the
  // result, potentially initiates the reset prompt flow.
  void FinishEvaluationFlow(scoped_ptr<EvaluationResults> results);

  // Begins the reset prompt flow by triggering the reset prompt, which consists
  // of two parts: (1.) the profile reset (pop-up) bubble, and (2.) a menu item
  // in the wrench menu (provided by a GlobalError).
  // The flow lasts until we receive a clear indication from the user about
  // whether or not they wish to reset their settings. This indication can come
  // in a variety of flavors:
  //  * taking definitive action (i.e. selecting either "Reset" or "No, thanks")
  //    in the pop-up reset bubble itself,
  //  * dismissing the bubble, but then selecting the wrench menu item, which
  //    takes them to the WebUI reset dialog in chrome://settings, and then the
  //    user can make their choice there,
  //  * the user going to the WebUI reset dialog by themself.
  // For the most part, the conclusion of the reset flow coincides with when the
  // reset prompt is dismissed, with the one exception being that the prompt is
  // closed as soon as the WebUI reset dialog is opened, we do not wait until
  // the user actually makes a choice in that dialog.
  void BeginResetPromptFlow();

  // Called back by the ProfileResetter once resetting the profile settings has
  // been completed, when requested by the user from inside the reset bubble.
  // Will dismiss the prompt and conclude the reset prompt flow.
  void OnProfileSettingsResetCompleted();

  // Reports the result of triggering the prompt through UMA. Virtual, so it can
  // be mocked out in tests to verify that the correct value is being reported.
  virtual void ReportPromptResult(PromptResult result);

  // Writes the memento values returned by the evaluation program to disk, and
  // then destroys |evaluation_results_|.
  void PersistMementos();

  // Concludes the reset prompt flow.
  void FinishResetPromptFlow();

  Profile* profile_;

  State state_;
  bool enumeration_of_loaded_modules_ready_;
  bool template_url_service_ready_;
  bool has_already_dismissed_prompt_;

  scoped_ptr<InputBuilder> input_builder_;
  std::string hash_seed_;
  std::string program_;

  scoped_ptr<EvaluationResults> evaluation_results_;

  bool should_show_reset_banner_;

  scoped_ptr<AutomaticProfileResetterDelegate> delegate_;
  scoped_refptr<base::TaskRunner> task_runner_for_waiting_;

  base::WeakPtrFactory<AutomaticProfileResetter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetter);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_H_
