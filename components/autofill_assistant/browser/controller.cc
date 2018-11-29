// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill_assistant {

namespace {

// Time between two periodic script checks.
static constexpr base::TimeDelta kPeriodicScriptCheckInterval =
    base::TimeDelta::FromSeconds(2);

// Number of script checks to run after a call to StartPeriodicScriptChecks.
static constexpr int kPeriodicScriptCheckCount = 10;

// Maximum number of script checks we should do before failing when trying to
// autostart.
static constexpr int kAutostartCheckCountLimit = 5;

// Caller parameter name.
static const char* const kCallerScriptParameterName = "CALLER";

// Cookie experiment name.
// TODO(crbug.com/806868): Introduce a dedicated experiment extra parameter to
// pass allow passing more than one experiment.
static const char* const kCookieExperimentName = "EXP_COOKIE";
// Website visited before parameter.
// Note: This parameter goes with the previous experiment name. I.e. it is only
// set when the cookie experiment is active.
static const char* const kWebsiteVisitedBeforeParameterName =
    "WEBSITE_VISITED_BEFORE";

static const char* const kTrueValue = "true";

}  // namespace

// static
void Controller::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<Client> client,
    std::unique_ptr<std::map<std::string, std::string>> parameters,
    const std::string& locale,
    const std::string& country_code) {
  // Get the key early since |client| will be invalidated when moved below.
  GURL server_url(client->GetServerUrl());
  DCHECK(server_url.is_valid());

  std::unique_ptr<Service> service = std::make_unique<Service>(
      client->GetApiKey(), server_url, web_contents->GetBrowserContext(),
      client->GetAccessTokenFetcher(), locale, country_code);
  new Controller(web_contents, std::move(client),
                 WebController::CreateForWebContents(web_contents),
                 std::move(service), std::move(parameters));
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

const std::map<std::string, std::string>& Controller::GetParameters() {
  return *parameters_;
}

autofill::PersonalDataManager* Controller::GetPersonalDataManager() {
  return client_->GetPersonalDataManager();
}

content::WebContents* Controller::GetWebContents() {
  return web_contents();
}

void Controller::SetTouchableElementArea(
    const std::vector<Selector>& elements) {
  touchable_element_area_.SetElements(elements);
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
      parameters_(std::move(parameters)),
      memory_(std::make_unique<ClientMemory>()),
      touchable_element_area_(web_controller_.get()),
      script_tracker_(std::make_unique<ScriptTracker>(/* delegate= */ this,
                                                      /* listener= */ this)),
      weak_ptr_factory_(this) {
  DCHECK(parameters_);

  // Only set the controller as the delegate if web_contents does not yet have
  // one.
  // TODO(crbug.com/806868): Find a better way to get a loading progress instead
  // of using the controller as a web_contents delegate. It may interfere with
  // an already existing delegate.
  if (web_contents->GetDelegate() == nullptr) {
    clear_web_contents_delegate_ = true;
    web_contents->SetDelegate(this);
  }

  GetUiController()->SetUiDelegate(this);
}

Controller::~Controller() {
  if (clear_web_contents_delegate_) {
    web_contents()->SetDelegate(nullptr);
  }
}

void Controller::GetOrCheckScripts(const GURL& url) {
  if (!started_ || script_tracker_->running()) {
    return;
  }

  if (script_domain_ != url.host()) {
    StopPeriodicScriptChecks();
    script_domain_ = url.host();
    service_->GetScriptsForUrl(
        url, *parameters_,
        base::BindOnce(&Controller::OnGetScripts, base::Unretained(this), url));
  } else {
    script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
    StartPeriodicScriptChecks();
  }
}

void Controller::StartPeriodicScriptChecks() {
  periodic_script_check_count_ = kPeriodicScriptCheckCount;
  // If periodic checks are running, setting periodic_script_check_count_ keeps
  // them running longer.
  if (periodic_script_check_scheduled_)
    return;
  periodic_script_check_scheduled_ = true;
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicScriptCheckInterval);
}

void Controller::StopPeriodicScriptChecks() {
  periodic_script_check_count_ = 0;
}

void Controller::OnPeriodicScriptCheck() {
  if (periodic_script_check_count_ <= 0) {
    DCHECK_EQ(0, periodic_script_check_count_);
    periodic_script_check_scheduled_ = false;
    return;
  }

  if (should_fail_after_checking_scripts_ &&
      ++total_script_check_count_ >= kAutostartCheckCountLimit) {
    should_fail_after_checking_scripts_ = false;
    GetUiController()->HideOverlay();
    GetUiController()->ShowStatusMessage(
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR));
    GetUiController()->ShutdownGracefully();
    return;
  }

  periodic_script_check_count_--;
  script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicScriptCheckInterval);
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
  script_tracker_->SetScripts(std::move(scripts));
  script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
  StartPeriodicScriptChecks();
}

void Controller::ExecuteScript(const std::string& script_path) {
  DCHECK(!script_tracker_->running());

  GetUiController()->ShowOverlay();
  touchable_element_area_.ClearElements();

  StopPeriodicScriptChecks();
  // Runnable scripts will be checked and reported if necessary after executing
  // the script.
  script_tracker_->ClearRunnableScripts();
  GetUiController()->UpdateScripts({});  // Clear scripts.
  // TODO(crbug.com/806868): Consider making ClearRunnableScripts part of
  // ExecuteScripts to simplify the controller.
  script_tracker_->ExecuteScript(
      script_path, base::BindOnce(&Controller::OnScriptExecuted,
                                  // script_tracker_ is owned by Controller.
                                  base::Unretained(this), script_path));
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  const ScriptExecutor::Result& result) {
  if (!allow_autostart_)
    GetUiController()->HideOverlay();
  if (!result.success) {
    LOG(ERROR) << "Failed to execute script " << script_path;
    GetUiController()->ShowStatusMessage(
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR));
    GetUiController()->ShutdownGracefully();
    return;
  }
  touchable_element_area_.SetElements(result.touchable_elements);
  switch (result.at_end) {
    case ScriptExecutor::SHUTDOWN:
      GetUiController()->Shutdown();  // indirectly deletes this
      return;

    case ScriptExecutor::SHUTDOWN_GRACEFULLY:
      GetWebController()->ClearCookie();
      GetUiController()->ShutdownGracefully();
      return;

    case ScriptExecutor::CLOSE_CUSTOM_TAB:
      GetUiController()->Close();
      return;

    case ScriptExecutor::RESTART:
      script_tracker_ = std::make_unique<ScriptTracker>(/* delegate= */ this,
                                                        /* listener= */ this);
      memory_ = std::make_unique<ClientMemory>();
      script_domain_ = "";
      break;

    case ScriptExecutor::CONTINUE:
      break;

    default:
      DLOG(ERROR) << "Unexpected value for at_end: " << result.at_end;
      break;
  }
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::GiveUp() {
  GetUiController()->ShowStatusMessage(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP));
  GetUiController()->ShutdownGracefully();
}

bool Controller::MaybeAutostartScript(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // We want to g through all runnable autostart interrupts first, one at a
  // time. To do that, always run highest priority autostartable interrupt from
  // runnable_script, which is ordered by priority.
  for (const auto& script : runnable_scripts) {
    if (script.autostart && script.interrupt) {
      std::string script_path = script.path;
      ExecuteScript(script_path);
      // making a copy of script.path is necessary, as ExecuteScript clears
      // runnable_scripts, so script.path will not survive until the end of
      // ExecuteScript.
      return true;
    }
  }

  // Under specific conditions, we can directly run a non-interrupt script
  // without first displaying it. This is meant to work only at the very
  // beginning, when no non-interrupt scripts have run, and only if there's
  // exactly one autostartable script.
  if (allow_autostart_) {
    int autostart_count = 0;
    std::string autostart_path;
    for (const auto& script : runnable_scripts) {
      if (script.autostart && !script.interrupt) {
        autostart_count++;
        autostart_path = script.path;
      }
    }
    if (autostart_count == 1) {
      allow_autostart_ = false;
      ExecuteScript(autostart_path);
      return true;
    }
  }
  return false;
}

void Controller::OnGetCookie(const GURL& initial_url, bool has_cookie) {
  if (has_cookie) {
    // This code is only active with the experiment parameter.
    parameters_->insert(
        std::make_pair(kWebsiteVisitedBeforeParameterName, kTrueValue));
    OnSetCookie(initial_url, has_cookie);
    return;
  }
  GetWebController()->SetCookie(
      initial_url.host(),
      base::BindOnce(&Controller::OnSetCookie,
                     // WebController is owned by Controller.
                     base::Unretained(this), initial_url));
}

void Controller::OnSetCookie(const GURL& initial_url, bool result) {
  DCHECK(result) << "Setting cookie failed";
  FinishStart(initial_url);
}

void Controller::FinishStart(const GURL& initial_url) {
  started_ = true;
  GetOrCheckScripts(initial_url);
  if (allow_autostart_) {
    auto iter = parameters_->find(kCallerScriptParameterName);
    // TODO(crbug.com/806868): Put back an explicit AUTOSTART parameter so we
    // don't need to know who calls us.
    if (iter != parameters_->end() && iter->second == "1") {
      should_fail_after_checking_scripts_ = true;
      GetUiController()->ShowOverlay();
      GetUiController()->ShowStatusMessage(l10n_util::GetStringFUTF8(
          IDS_AUTOFILL_ASSISTANT_LOADING,
          base::UTF8ToUTF16(web_contents()->GetVisibleURL().host())));
    }
  }

  touchable_element_area_.SetOnUpdate(base::BindRepeating(
      &UiController::UpdateTouchableArea,
      // Unretained is safe, since touchable_element_area_ is guaranteed to be
      // deleted before the UI controller.
      base::Unretained(GetUiController())));
}

void Controller::Start(const GURL& initialUrl) {
  DCHECK(initialUrl.is_valid());
  if (IsCookieExperimentEnabled()) {
    GetWebController()->HasCookie(
        base::BindOnce(&Controller::OnGetCookie,
                       // WebController is owned by Controller.
                       base::Unretained(this), initialUrl));
  } else {
    FinishStart(initialUrl);
  }
}

void Controller::OnClickOverlay() {
  GetUiController()->HideOverlay();
  // TODO(crbug.com/806868): Stop executing scripts.
}

void Controller::OnDestroy() {
  delete this;
}

bool Controller::Terminate() {
  StopPeriodicScriptChecks();
  return script_tracker_->Terminate();
}

void Controller::OnScriptSelected(const std::string& script_path) {
  DCHECK(!script_path.empty());

  // This is a script selected from the UI, so it should disable autostart.
  allow_autostart_ = false;

  ExecuteScript(script_path);
}

void Controller::ScrollBy(float distanceXRatio, float distanceYRatio) {
  GetWebController()->ScrollBy(distanceXRatio, distanceYRatio);
  touchable_element_area_.UpdatePositions();
}

void Controller::UpdateTouchableArea() {
  touchable_element_area_.UpdatePositions();
}

std::string Controller::GetDebugContext() {
  base::Value dict(base::Value::Type::DICTIONARY);

  std::vector<base::Value> parameters_js;
  for (const auto& entry : *parameters_) {
    base::Value parameter_js = base::Value(base::Value::Type::DICTIONARY);
    parameter_js.SetKey(entry.first, base::Value(entry.second));
    parameters_js.push_back(std::move(parameter_js));
  }
  dict.SetKey("parameters", base::Value(parameters_js));
  dict.SetKey("scripts", script_tracker_->GetDebugContext());

  std::string output_js;
  base::JSONWriter::Write(dict, &output_js);
  return output_js;
}

void Controller::OnNoRunnableScriptsAnymore() {
  if (script_tracker_->running())
    return;

  // We're navigated to a page that has no scripts or the scripts have reached a
  // state from which they cannot recover through a DOM change.
  GiveUp();
  return;
}

void Controller::OnRunnableScriptsChanged(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker_->running())
    return;

  if (!runnable_scripts.empty()) {
    should_fail_after_checking_scripts_ = false;
    GetUiController()->HideOverlay();
  }

  if (MaybeAutostartScript(runnable_scripts)) {
    return;
  }

  // Show the initial prompt if available.
  for (const auto& script : runnable_scripts) {
    // runnable_scripts is ordered by priority.
    if (!script.initial_prompt.empty()) {
      GetUiController()->ShowStatusMessage(script.initial_prompt);
      break;
    }
  }

  // Update the set of scripts in the UI.
  std::vector<ScriptHandle> scripts_to_update;
  for (const auto& script : runnable_scripts) {
    if (!script.autostart && !script.name.empty()) {
      scripts_to_update.emplace_back(script);
    }
  }
  GetUiController()->UpdateScripts(scripts_to_update);
}

void Controller::DidAttachInterstitialPage() {
  GetUiController()->Shutdown();
}

void Controller::DidGetUserInteraction(const blink::WebInputEvent::Type type) {
  switch (type) {
    case blink::WebInputEvent::kTouchStart:
    case blink::WebInputEvent::kGestureTapDown:
      if (!script_tracker_->running()) {
        script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
        StartPeriodicScriptChecks();
      }
      break;

    default:
      break;
  }
}

void Controller::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  // validated_url might not be the page URL. Ignore it and always check the
  // last committed url.
  // Note that we also check for scripts in LoadProgressChanged below. This is
  // the last attempt and occurs later than a load progress of 1.0.
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // The following types of navigations are allowed for the main frame:
  //  - first-time URL load
  //  - script-directed navigation, while a script is running unless
  //    there's a touchable area.
  //  - server redirections, which might happen outside of a script, but
  //    because of a load triggered by a previously-running script.
  //  - same-document modifications, which might happen automatically
  //  - javascript-initiated navigation or refresh
  //  - navigation by clicking on a link
  //  In the last two cases, autofill assistant might still give up later on if
  //  it discovers that the new page has no scripts.
  //
  // Everything else, such as going back to a previous page, or refreshing the
  // page is considered an end condition.
  if (navigation_handle->IsInMainFrame() &&
      web_contents()->GetLastCommittedURL().is_valid() &&
      !script_tracker_->running() && !navigation_handle->WasServerRedirect() &&
      !navigation_handle->IsSameDocument() &&
      !navigation_handle->IsRendererInitiated()) {
    GiveUp();
    return;
  }

  // Special case: during a prompt, forbid render-initiated navigation. This is
  // necessary as there won't be any script lookup to tell us whether the
  // destination page is acceptable.
  if (script_tracker_->running() && touchable_element_area_.HasElements() &&
      navigation_handle->IsRendererInitiated()) {
    GiveUp();
    return;
  }
}

void Controller::DocumentAvailableInMainFrame() {
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::RenderProcessGone(base::TerminationStatus status) {
  GetUiController()->Shutdown();
}

void Controller::WebContentsDestroyed() {
  OnDestroy();
}

void Controller::LoadProgressChanged(content::WebContents* source,
                                     double progress) {
  int percent = 100 * progress;
  // We wait for a page to be at least 40 percent loaded. Then a new
  // precondition check is started every additional 20 percent.
  if (percent >= 40 && percent % 20 == 0) {
    DCHECK(web_contents()->GetLastCommittedURL().is_valid());
    // In order to show available scripts as early as possible we start checking
    // preconditions when the page has not yet fully loaded. This can lead to
    // the behavior where scripts are being added sequentially instead of all
    // at the same time. Also, depending on the progress values, we may never
    // actually get here. In that case the only check will happen in
    // DidFinishLoad.
    GetOrCheckScripts(web_contents()->GetLastCommittedURL());
  }
}

bool Controller::IsCookieExperimentEnabled() const {
  auto iter = parameters_->find(kCookieExperimentName);
  return iter != parameters_->end() && iter->second == "1";
}

}  // namespace autofill_assistant
