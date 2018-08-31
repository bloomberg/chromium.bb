// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill_assistant {
// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class Controller : public ScriptExecutorDelegate,
                   public UiDelegate,
                   private content::WebContentsObserver {
 public:
  static void CreateAndStartForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<UiController> ui_controller);

  // Overrides ScriptExecutorDelegate:
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;

 private:
  Controller(content::WebContents* web_contents,
             std::unique_ptr<UiController> ui_controller);
  ~Controller() override;

  void GetScripts();
  void OnGetScripts(bool result, const std::string& response);

  // Overrides content::UiDelegate:
  void OnClickOverlay() override;
  void OnDestroy() override;

  // Overrides content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

  std::unique_ptr<UiController> ui_controller_;
  std::unique_ptr<WebController> web_controller_;
  std::unique_ptr<Service> service_;
  std::map<Script*, std::unique_ptr<Script>> scripts_;
  ClientMemory memory_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
