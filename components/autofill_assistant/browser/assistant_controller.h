// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_CONTROLLER_H_

#include "components/autofill_assistant/browser/assistant_script.h"
#include "components/autofill_assistant/browser/assistant_script_executor_delegate.h"
#include "components/autofill_assistant/browser/assistant_service.h"
#include "components/autofill_assistant/browser/assistant_ui_controller.h"
#include "components/autofill_assistant/browser/assistant_ui_delegate.h"
#include "components/autofill_assistant/browser/assistant_web_controller.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill_assistant {
// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class AssistantController : public AssistantScriptExecutorDelegate,
                            public AssistantUiDelegate,
                            private content::WebContentsObserver {
 public:
  static void CreateAndStartForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<AssistantUiController> ui_controller);

  // Overrides AssistantScriptExecutorDelegate:
  AssistantService* GetAssistantService() override;
  AssistantUiController* GetAssistantUiController() override;
  AssistantWebController* GetAssistantWebController() override;

 private:
  AssistantController(content::WebContents* web_contents,
                      std::unique_ptr<AssistantUiController> ui_controller);
  ~AssistantController() override;

  void GetAssistantScripts();
  void OnGetAssistantScripts(bool result, const std::string& response);

  // Overrides content::AssistantUiDelegate:
  void OnClickOverlay() override;
  void OnDestroy() override;

  // Overrides content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

  std::unique_ptr<AssistantUiController> assistant_ui_controller_;
  std::unique_ptr<AssistantWebController> assistant_web_controller_;
  std::unique_ptr<AssistantService> assistant_service_;
  std::map<AssistantScript*, std::unique_ptr<AssistantScript>>
      assistant_scripts_;

  DISALLOW_COPY_AND_ASSIGN(AssistantController);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_CONTROLLER_H_