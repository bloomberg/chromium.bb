// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_controller.h"

#include "components/autofill_assistant/browser/assistant_protocol_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
// static
void AssistantController::CreateAndStartForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<AssistantUiController> ui_controller) {
  new AssistantController(web_contents, std::move(ui_controller));
}

AssistantService* AssistantController::GetAssistantService() {
  return assistant_service_.get();
}

AssistantUiController* AssistantController::GetAssistantUiController() {
  return assistant_ui_controller_.get();
}

AssistantWebController* AssistantController::GetAssistantWebController() {
  return assistant_web_controller_.get();
}

AssistantController::AssistantController(
    content::WebContents* web_contents,
    std::unique_ptr<AssistantUiController> ui_controller)
    : content::WebContentsObserver(web_contents),
      assistant_ui_controller_(std::move(ui_controller)),
      assistant_web_controller_(std::make_unique<AssistantWebController>()),
      assistant_service_(std::make_unique<AssistantService>(
          web_contents->GetBrowserContext())) {
  assistant_ui_controller_->SetUiDelegate(this);
  assistant_ui_controller_->ShowOverlay();
  if (!web_contents->IsLoading()) {
    GetAssistantScripts();
  }
}

AssistantController::~AssistantController() {}

void AssistantController::GetAssistantScripts() {
  assistant_service_->GetAssistantScriptsForUrl(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&AssistantController::OnGetAssistantScripts,
                     base::Unretained(this)));
}

void AssistantController::OnGetAssistantScripts(bool result,
                                                const std::string& response) {
  if (!result) {
    LOG(ERROR) << "Failed to get assistant scripts for URL "
               << web_contents()->GetLastCommittedURL().spec();
    // TODO(crbug.com/806868): Terminate Autofill Assistant.
    return;
  }
  bool parse_result = AssistantProtocolUtils::ParseAssistantScripts(
      response, &assistant_scripts_);
  DCHECK(parse_result);
  // TODO(crbug.com/806868): Present assistant scripts if necessary or auto
  // start a script.
}

void AssistantController::OnClickOverlay() {
  assistant_ui_controller_->HideOverlay();
  // TODO(crbug.com/806868): Stop executing scripts.
}

void AssistantController::OnDestroy() {
  delete this;
}

void AssistantController::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // TODO(crbug.com/806868): Find a better time to get and update assistant
  // scripts.
  GetAssistantScripts();
}

void AssistantController::WebContentsDestroyed() {
  OnDestroy();
}

}  // namespace autofill_assistant