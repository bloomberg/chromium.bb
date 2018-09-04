// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
// static
void Controller::CreateAndStartForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<Client> client) {
  new Controller(web_contents, std::move(client));
}

Service* Controller::GetService() {
  return service_.get();
}

UiController* Controller::GetUiController() {
  return client_->GetUiController();
}

WebController* Controller::GetWebController() {
  return web_controller_.get();
}

ClientMemory* Controller::GetClientMemory() {
  return &memory_;
}

Controller::Controller(content::WebContents* web_contents,
                       std::unique_ptr<Client> client)
    : content::WebContentsObserver(web_contents),
      client_(std::move(client)),
      web_controller_(WebController::CreateForWebContents(web_contents)),
      service_(std::make_unique<Service>(client_->GetApiKey(),
                                         web_contents->GetBrowserContext())) {
  GetUiController()->SetUiDelegate(this);
  GetUiController()->ShowOverlay();
  if (!web_contents->IsLoading()) {
    GetScripts();
  }
}

Controller::~Controller() {}

void Controller::GetScripts() {
  service_->GetScriptsForUrl(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&Controller::OnGetScripts, base::Unretained(this)));
}

void Controller::OnGetScripts(bool result, const std::string& response) {
  if (!result) {
    LOG(ERROR) << "Failed to get assistant scripts for URL "
               << web_contents()->GetLastCommittedURL().spec();
    // TODO(crbug.com/806868): Terminate Autofill Assistant.
    return;
  }
  bool parse_result = ProtocolUtils::ParseScripts(response, &scripts_);
  DCHECK(parse_result);
  // TODO(crbug.com/806868): Present assistant scripts if necessary or auto
  // start a script.
}

void Controller::OnClickOverlay() {
  GetUiController()->HideOverlay();
  // TODO(crbug.com/806868): Stop executing scripts.
}

void Controller::OnDestroy() {
  delete this;
}

void Controller::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  // TODO(crbug.com/806868): Find a better time to get and update assistant
  // scripts.
  GetScripts();
}

void Controller::WebContentsDestroyed() {
  OnDestroy();
}

}  // namespace autofill_assistant
