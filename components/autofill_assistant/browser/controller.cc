// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>
#include <vector>

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
                                         web_contents->GetBrowserContext())),
      script_tracker_(std::make_unique<ScriptTracker>(this, this)) {
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
  std::vector<std::unique_ptr<Script>> scripts;
  bool parse_result = ProtocolUtils::ParseScripts(response, &scripts);
  DCHECK(parse_result);
  script_tracker_->SetAndCheckScripts(std::move(scripts));
}

void Controller::OnClickOverlay() {
  GetUiController()->HideOverlay();
  // TODO(crbug.com/806868): Stop executing scripts.
}

void Controller::OnDestroy() {
  delete this;
}

void Controller::OnRunnableScriptsChanged() {
  // TODO(crbug.com/806868): Take the set of runnable script from the tracker
  // and make them available for selection. Run the selected script.
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
