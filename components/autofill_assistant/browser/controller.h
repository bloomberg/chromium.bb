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
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.h"
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
  static void CreateForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<Client> client,
      std::unique_ptr<std::map<std::string, std::string>> parameters,
      const std::string& locale,
      const std::string& country_code);

  // Overrides ScriptExecutorDelegate:
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;
  const std::map<std::string, std::string>& GetParameters() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  content::WebContents* GetWebContents() override;
  void SetTouchableElementArea(const std::vector<Selector>& elements) override;

  bool IsCookieExperimentEnabled() const;

 private:
  friend ControllerTest;

  Controller(content::WebContents* web_contents,
             std::unique_ptr<Client> client,
             std::unique_ptr<WebController> web_controller,
             std::unique_ptr<Service> service,
             std::unique_ptr<std::map<std::string, std::string>> parameters);
  ~Controller() override;

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
  void GiveUp();

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

  // Overrides content::UiDelegate:
  void Start(const GURL& initialUrl) override;
  void OnClickOverlay() override;
  void OnDestroy() override;
  void ScrollBy(float distanceXRatio, float distanceYRatio) override;
  void UpdateTouchableArea() override;
  void OnScriptSelected(const std::string& script_path) override;
  std::string GetDebugContext() override;
  bool Terminate() override;

  // Overrides ScriptTracker::Listener:
  void OnNoRunnableScriptsAnymore() override;
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override;

  // Overrides content::WebContentsObserver:
  void DidAttachInterstitialPage() override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;

  // Overrides content::WebContentsDelegate:
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;

  std::unique_ptr<Client> client_;
  std::unique_ptr<WebController> web_controller_;
  std::unique_ptr<Service> service_;
  std::unique_ptr<ScriptTracker> script_tracker_;
  std::unique_ptr<std::map<std::string, std::string>> parameters_;

  // Domain of the last URL the controller requested scripts from.
  std::string script_domain_;
  std::unique_ptr<ClientMemory> memory_;
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
  ElementArea touchable_element_area_;

  bool started_ = false;

  base::WeakPtrFactory<Controller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
