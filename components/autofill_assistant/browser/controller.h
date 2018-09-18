// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/web_controller.h"
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
                   private content::WebContentsObserver {
 public:
  static void CreateAndStartForWebContents(content::WebContents* web_contents,
                                           std::unique_ptr<Client> client);

  // Overrides ScriptExecutorDelegate:
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;

 private:
  friend ControllerTest;

  Controller(content::WebContents* web_contents,
             std::unique_ptr<Client> client,
             std::unique_ptr<WebController> web_controller,
             std::unique_ptr<Service> service);
  ~Controller() override;

  void GetOrCheckScripts(const GURL& url);
  void OnGetScripts(const GURL& url, bool result, const std::string& response);
  void OnScriptChosen(const std::string& script_path);
  void OnScriptExecuted(const std::string& script_path, bool success);

  // Overrides content::UiDelegate:
  void OnClickOverlay() override;
  void OnDestroy() override;
  void OnScriptSelected(const std::string& script_path) override;

  // Overrides ScriptTracker::Listener:
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override;

  // Overrides content::WebContentsObserver:
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

  std::unique_ptr<Client> client_;
  std::unique_ptr<WebController> web_controller_;
  std::unique_ptr<Service> service_;
  std::unique_ptr<ScriptTracker> script_tracker_;

  // Domain of the last URL the controller requested scripts from.
  std::string script_domain_;
  std::unique_ptr<ClientMemory> memory_;
  bool allow_autostart_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
