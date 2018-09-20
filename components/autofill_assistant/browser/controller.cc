// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>

#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
// static
void Controller::CreateAndStartForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<Client> client,
    std::unique_ptr<std::map<std::string, std::string>> parameters) {
  // Get the key early since |client| will be invalidated when moved below.
  const std::string api_key = client->GetApiKey();
  new Controller(
      web_contents, std::move(client),
      WebController::CreateForWebContents(web_contents),
      std::make_unique<Service>(api_key, web_contents->GetBrowserContext()),
      std::move(parameters));
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
  return memory_.get();
}

Controller::Controller(
    content::WebContents* web_contents,
    std::unique_ptr<Client> client,
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<Service> service,
    std::unique_ptr<std::map<std::string, std::string>> parameters)
    : content::WebContentsObserver(web_contents),
      client_(std::move(client)),
      web_controller_(std::move(web_controller)),
      service_(std::move(service)),
      script_tracker_(
          std::make_unique<ScriptTracker>(this, this, std::move(parameters))),
      memory_(std::make_unique<ClientMemory>()),
      allow_autostart_(true) {
  GetUiController()->SetUiDelegate(this);
  GetUiController()->ShowOverlay();
  if (!web_contents->IsLoading()) {
    GetOrCheckScripts(web_contents->GetLastCommittedURL());
  }
}

Controller::~Controller() {}

void Controller::GetOrCheckScripts(const GURL& url) {
  if (script_tracker_->running())
    return;

  if (script_domain_ != url.host()) {
    script_domain_ = url.host();
    service_->GetScriptsForUrl(
        url,
        base::BindOnce(&Controller::OnGetScripts, base::Unretained(this), url));
  } else {
    script_tracker_->CheckScripts();
  }
}

void Controller::OnGetScripts(const GURL& url,
                              bool result,
                              const std::string& response) {
  // If the domain of the current URL changed since the request was sent, the
  // response is not relevant anymore and can be safely discarded.
  if (url.host() != script_domain_)
    return;

  if (!result) {
    LOG(ERROR) << "Failed to get assistant scripts for URL " << url.spec();
    // TODO(crbug.com/806868): Terminate Autofill Assistant.
    return;
  }

  std::vector<std::unique_ptr<Script>> scripts;
  bool parse_result = ProtocolUtils::ParseScripts(response, &scripts);
  DCHECK(parse_result);
  script_tracker_->SetAndCheckScripts(std::move(scripts));
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  bool success) {
  GetUiController()->HideOverlay();
  if (!success) {
    LOG(ERROR) << "Failed to execute script " << script_path;
    // TODO(crbug.com/806868): Handle script execution failure.
  }

  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::OnClickOverlay() {
  GetUiController()->HideOverlay();
  // TODO(crbug.com/806868): Stop executing scripts.
}

void Controller::OnScriptSelected(const std::string& script_path) {
  DCHECK(!script_path.empty());

  GetUiController()->ShowOverlay();
  allow_autostart_ = false;  // Only ever autostart the very first script.
  script_tracker_->ExecuteScript(
      script_path, base::BindOnce(&Controller::OnScriptExecuted,
                                  // script_tracker_ is owned by Controller.
                                  base::Unretained(this), script_path));
}

void Controller::OnDestroy() {
  delete this;
}

void Controller::DidGetUserInteraction(const blink::WebInputEvent::Type type) {
  switch (type) {
    case blink::WebInputEvent::kGestureLongTap:
    case blink::WebInputEvent::kGestureTap:
      // Disable autostart after interaction with the web page.
      allow_autostart_ = false;

      if (!script_tracker_->running())
        script_tracker_->CheckScripts();
      break;

    default:
      break;
  }
}

void Controller::OnRunnableScriptsChanged(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker_->running())
    return;

  // Under specific conditions, we can directly run a script without first
  // displaying it. This is meant to work only at the very beginning, when
  // no scripts have run, there has been no interaction with the webpage and
  // only if there's exactly one runnable script, flagged for autostart.
  if (allow_autostart_ && runnable_scripts.size() == 1 &&
      runnable_scripts[0].autostart) {
    OnScriptSelected(runnable_scripts[0].path);
    return;
  }

  GetUiController()->UpdateScripts(runnable_scripts);
}

void Controller::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  // TODO(crbug.com/806868): Find a better time to get and update assistant
  // scripts.
  GetOrCheckScripts(validated_url);
}

void Controller::WebContentsDestroyed() {
  OnDestroy();
}

}  // namespace autofill_assistant
