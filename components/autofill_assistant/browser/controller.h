// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/element_area.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill_assistant {
class ControllerTest;

// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class Controller : public ScriptExecutorDelegate,
                   public UiDelegate,
                   public ScriptTracker::Listener,
                   private content::WebContentsObserver,
                   private content::WebContentsDelegate {
 public:
  // |web_contents| and |client| must remain valid for the lifetime of the
  // instance.
  Controller(content::WebContents* web_contents, Client* client);
  ~Controller() override;

  // Called when autofill assistant can start executing scripts.
  void Start(const GURL& initialUrl,
             const std::map<std::string, std::string>& parameters);

  // Initiates a clean shutdown.
  //
  // This function returns false when it needs more time to properly shut down
  // the script tracker. It usually means that it either has to wait for a
  // script to find an appropriate moment to suspend execution or wait for a
  // script checking round to complete.
  //
  // A caller is expected to try again later when this function returns false. A
  // return value of true means that the scrip tracker can safely be destroyed.
  //
  // TODO(crbug.com/806868): Instead of this safety net, the proper fix is to
  // switch to weak pointers everywhere so that dangling callbacks are not an
  // issue.
  bool Terminate();

  // Overrides ScriptExecutorDelegate:
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;
  const std::map<std::string, std::string>& GetParameters() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  content::WebContents* GetWebContents() override;
  void SetTouchableElementArea(const ElementAreaProto& area) override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetDetails(const Details& details) override;
  void ClearDetails() override;
  void EnterState(AutofillAssistantState state) override;
  bool IsCookieExperimentEnabled() const;

 private:
  friend ControllerTest;

  void SetWebControllerAndServiceForTest(
      std::unique_ptr<WebController> web_controller,
      std::unique_ptr<Service> service);

  void GetOrCheckScripts(const GURL& url);
  void OnGetScripts(const GURL& url, bool result, const std::string& response);
  void ExecuteScript(const std::string& script_path);
  void OnScriptExecuted(const std::string& script_path,
                        const ScriptExecutor::Result& result);

  // Check script preconditions every few seconds for a certain number of times.
  // If checks are already running, StartPeriodicScriptChecks resets the count.
  //
  // TODO(crbug.com/806868): Find a better solution. This is a brute-force
  // solution that reacts slowly to changes.
  void StartPeriodicScriptChecks();
  void StopPeriodicScriptChecks();
  void OnPeriodicScriptCheck();

  // Shows the given message and stops the controller with |reason|.
  void OnFatalError(const std::string& error_message,
                    Metrics::DropOutReason reason);

  // Runs autostart scripts from |runnable_scripts|, if the conditions are
  // right. Returns true if a script was auto-started.
  bool MaybeAutostartScript(const std::vector<ScriptHandle>& runnable_scripts);

  // Autofill Assistant cookie logic.
  //
  // On startup of the controller we set a cookie. If a cookie already existed
  // for the intial URL, we show a warning that the website has already been
  // visited and could contain old data. The cookie is cleared (or expires) when
  // a script terminated with a Stop action.
  void OnGetCookie(const GURL& initial_url, bool has_cookie);
  void OnSetCookie(const GURL& initial_url, bool result);
  void FinishStart(const GURL& initial_url);
  void MaybeSetInitialDetails();

  // Called when a script is selected.
  void OnScriptSelected(const std::string& script_path);

  // Overrides autofill_assistant::UiDelegate:
  AutofillAssistantState GetState() override;
  void UpdateTouchableArea() override;
  void OnUserInteractionInsideTouchableArea() override;
  const Details* GetDetails() const override;
  std::string GetDebugContext() override;
  Metrics::DropOutReason GetDropOutReason() const override;

  // Overrides ScriptTracker::Listener:
  void OnNoRunnableScriptsAnymore() override;
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override;

  // Overrides content::WebContentsObserver:
  void DidAttachInterstitialPage() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void RenderProcessGone(base::TerminationStatus status) override;

  // Overrides content::WebContentsDelegate:
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;

  ElementArea* touchable_element_area();
  ScriptTracker* script_tracker();

  Client* const client_;

  // Lazily instantiate in GetWebController().
  std::unique_ptr<WebController> web_controller_;

  // Lazily instantiate in GetService().
  std::unique_ptr<Service> service_;
  std::map<std::string, std::string> parameters_;

  // Lazily instantiate in GetClientMemory().
  std::unique_ptr<ClientMemory> memory_;

  AutofillAssistantState state_ = AutofillAssistantState::INACTIVE;

  // Domain of the last URL the controller requested scripts from.
  std::string script_domain_;
  bool allow_autostart_ = true;

  // Whether a task for periodic checks is scheduled.
  bool periodic_script_check_scheduled_ = false;

  // Number of remaining periodic checks.
  int periodic_script_check_count_ = 0;
  int total_script_check_count_ = 0;

  // Whether to clear the web_contents delegate when the controller is
  // destroyed.
  bool clear_web_contents_delegate_ = false;

  // Whether we should hide the overlay and show an error message after a first
  // unsuccessful round of preconditions checking.
  bool should_fail_after_checking_scripts_ = false;

  // Area of the screen that corresponds to the current set of touchable
  // elements.
  // Lazily instantiate in touchable_element_area().
  std::unique_ptr<ElementArea> touchable_element_area_;

  // Current status message, may be empty.
  std::string status_message_;

  // Current details, may be null.
  std::unique_ptr<Details> details_;

  // Flag indicates whether it is ready to fetch and execute scripts.
  bool started_ = false;

  // Drop out reason set when the controller enters a STOPPED state.
  Metrics::DropOutReason stop_reason_ = Metrics::AA_START;

  // Tracks scripts and script execution. It's kept at the end, as it tend to
  // depend on everything the controller support, through script and script
  // actions.
  // Lazily instantiate in script_tracker().
  std::unique_ptr<ScriptTracker> script_tracker_;

  base::WeakPtrFactory<Controller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
