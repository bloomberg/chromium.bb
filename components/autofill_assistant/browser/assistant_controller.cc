// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_controller.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
// static
void AssistantController::CreateAndStartForWebContents(
    content::WebContents* web_contents) {
  new AssistantController(web_contents);
}

AssistantService* AssistantController::GetAssistantService() {
  return assistant_service_.get();
}

AssistantController::AssistantController(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      assistant_service_(std::make_unique<AssistantService>(
          web_contents->GetBrowserContext())) {
  if (!web_contents->IsLoading()) {
    GetAssistantScripts();
  }
}

AssistantController::~AssistantController() {}

void AssistantController::OnGetAssistantScripts(
    AssistantService::AssistantScripts scripts) {
  assistant_scripts_ = std::move(scripts);
}

void AssistantController::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // TODO(crbug.com/806868): Find a better time to get and update assistant
  // scripts.
  GetAssistantScripts();
}

void AssistantController::WebContentsDestroyed() {
  delete this;
}

void AssistantController::GetAssistantScripts() {
  assistant_service_->GetAssistantScriptsForUrl(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&AssistantController::OnGetAssistantScripts,
                     base::Unretained(this)));
}

}  // namespace autofill_assistant