// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/metrics.h"
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

// The initial progress to set when autostarting and showing the "Loading..."
// message.
static constexpr int kAutostartInitialProgress = 10;

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

Controller::Controller(content::WebContents* web_contents, Client* client)
    : content::WebContentsObserver(web_contents),
      client_(client),
      weak_ptr_factory_(this) {
  // Only set the controller as the delegate if web_contents does not yet have
  // one.
  // TODO(crbug.com/806868): Find a better way to get a loading progress instead
  // of using the controller as a web_contents delegate. It may interfere with
  // an already existing delegate.
  if (web_contents->GetDelegate() == nullptr) {
    clear_web_contents_delegate_ = true;
    web_contents->SetDelegate(this);
  }
}

Controller::~Controller() {
  if (clear_web_contents_delegate_) {
    web_contents()->SetDelegate(nullptr);
  }
}

Service* Controller::GetService() {
  if (!service_) {
    service_ = Service::Create(web_contents()->GetBrowserContext(), client_);
  }
  return service_.get();
}

UiController* Controller::GetUiController() {
  return client_->GetUiController();
}

WebController* Controller::GetWebController() {
  if (!web_controller_) {
    web_controller_ = WebController::CreateForWebContents(web_contents());
  }
  return web_controller_.get();
}

ClientMemory* Controller::GetClientMemory() {
  if (!memory_) {
    memory_ = std::make_unique<ClientMemory>();
  }
  return memory_.get();
}

const std::map<std::string, std::string>& Controller::GetParameters() {
  return parameters_;
}

autofill::PersonalDataManager* Controller::GetPersonalDataManager() {
  return client_->GetPersonalDataManager();
}

content::WebContents* Controller::GetWebContents() {
  return web_contents();
}

void Controller::SetTouchableElementArea(const ElementAreaProto& area) {
  touchable_element_area()->SetFromProto(area);
}

void Controller::SetStatusMessage(const std::string& message) {
  status_message_ = message;
  GetUiController()->OnStatusMessageChanged(message);
}

std::string Controller::GetStatusMessage() const {
  return status_message_;
}

void Controller::SetDetails(const Details& details) {
  if (!details_) {
    details_ = std::make_unique<Details>();
  }
  *details_ = details;
  GetUiController()->OnDetailsChanged(details_.get());
}

void Controller::ClearDetails() {
  details_.reset();
  GetUiController()->OnDetailsChanged(nullptr);
}

const Details* Controller::GetDetails() const {
  return details_.get();
}

int Controller::GetProgress() const {
  return progress_;
}

void Controller::SetProgress(int progress) {
  // Progress can only increase.
  if (progress_ >= progress)
    return;

  progress_ = progress;
  GetUiController()->OnProgressChanged(progress);
}

const std::vector<Chip>& Controller::GetChips() const {
  static const base::NoDestructor<std::vector<Chip>> no_chips_;
  return chips_ ? *chips_ : *no_chips_;
}

void Controller::SetChips(std::unique_ptr<std::vector<Chip>> chips) {
  if (!chips || chips->empty()) {
    chips_.reset();
  } else {
    chips_ = std::move(chips);
  }
  GetUiController()->OnChipsChanged(GetChips());
}

void Controller::SelectChip(int chip_index) {
  if (!chips_ || chip_index < 0 ||
      static_cast<size_t>(chip_index) >= chips_->size()) {
    NOTREACHED() << "Invalid chip index: " << chip_index;
    return;
  }
  auto callback = std::move((*chips_)[chip_index].callback);
  SetChips(nullptr);
  std::move(callback).Run();
}

void Controller::StopAndShutdown(Metrics::DropOutReason reason) {
  ClearDetails();
  SetChips(nullptr);
  SetPaymentRequestOptions(nullptr);
  EnterState(AutofillAssistantState::STOPPED);
  client_->Shutdown(reason);
}

void Controller::EnterState(AutofillAssistantState state) {
  if (state_ == state)
    return;

  DCHECK_NE(state_, AutofillAssistantState::STOPPED)
      << "Unexpected transition from " << state_ << " to " << state;

  state_ = state;
  GetUiController()->OnStateChanged(state);
}

void Controller::SetWebControllerAndServiceForTest(
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<Service> service) {
  web_controller_ = std::move(web_controller);
  service_ = std::move(service);
}

void Controller::GetOrCheckScripts(const GURL& url) {
  if (!started_ || script_tracker()->running()) {
    return;
  }

  if (script_domain_ != url.host()) {
    StopPeriodicScriptChecks();
    script_domain_ = url.host();
    DVLOG(2) << "GetScripts for " << script_domain_;
    GetService()->GetScriptsForUrl(
        url, parameters_,
        base::BindOnce(&Controller::OnGetScripts, base::Unretained(this), url));
  } else {
    script_tracker()->CheckScripts(kPeriodicScriptCheckInterval);
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
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::AUTOSTART_TIMEOUT);
    return;
  }

  periodic_script_check_count_--;
  script_tracker()->CheckScripts(kPeriodicScriptCheckInterval);
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicScriptCheckInterval);
}

void Controller::OnGetScripts(const GURL& url,
                              bool result,
                              const std::string& response) {
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  // If the domain of the current URL changed since the request was sent, the
  // response is not relevant anymore and can be safely discarded.
  if (url.host() != script_domain_)
    return;

  if (!result) {
    DVLOG(1) << "Failed to get assistant scripts for URL " << url.spec();
    // TODO(crbug.com/806868): Terminate Autofill Assistant.
    return;
  }

  std::vector<std::unique_ptr<Script>> scripts;
  bool parse_result = ProtocolUtils::ParseScripts(response, &scripts);
  DVLOG(2) << __func__ << " from " << url.host() << " returned "
           << scripts.size() << " scripts";
  DCHECK(parse_result);
  script_tracker()->SetScripts(std::move(scripts));
  script_tracker()->CheckScripts(kPeriodicScriptCheckInterval);
  StartPeriodicScriptChecks();
}

void Controller::ExecuteScript(const std::string& script_path) {
  DCHECK(!script_tracker()->running());
  EnterState(AutofillAssistantState::RUNNING);

  touchable_element_area()->Clear();

  StopPeriodicScriptChecks();
  // Runnable scripts will be checked and reported if necessary after executing
  // the script.
  script_tracker_->ClearRunnableScripts();
  SetChips(nullptr);
  // TODO(crbug.com/806868): Consider making ClearRunnableScripts part of
  // ExecuteScripts to simplify the controller.
  script_tracker()->ExecuteScript(
      script_path, base::BindOnce(&Controller::OnScriptExecuted,
                                  // script_tracker_ is owned by Controller.
                                  base::Unretained(this), script_path));
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  const ScriptExecutor::Result& result) {
  if (!result.success) {
    DVLOG(1) << "Failed to execute script " << script_path;
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::SCRIPT_FAILED);
    return;
  }

  if (result.touchable_element_area) {
    touchable_element_area()->SetFromProto(*result.touchable_element_area);
  }

  switch (result.at_end) {
    case ScriptExecutor::SHUTDOWN:
      client_->Shutdown(Metrics::SCRIPT_SHUTDOWN);
      return;

    case ScriptExecutor::TERMINATE:
      // TODO(crbug.com/806868): Distinguish shutdown from terminate: Users
      // should be allowed to undo shutdown, but not terminate.
      //
      // There should have been a previous call to Terminate() that set the
      // reason, thus SAFETY_NET_TERMINATE should never be logged, unless
      // there's a bug.
      DCHECK_NE(terminate_reason_, Metrics::SAFETY_NET_TERMINATE);
      client_->Shutdown(terminate_reason_);
      return;

    case ScriptExecutor::SHUTDOWN_GRACEFULLY:
      GetWebController()->ClearCookie();
      StopAndShutdown(Metrics::SCRIPT_SHUTDOWN);
      return;

    case ScriptExecutor::CLOSE_CUSTOM_TAB:
      client_->Shutdown(Metrics::CUSTOM_TAB_CLOSED);
      return;

    case ScriptExecutor::RESTART:
      script_tracker_.reset();
      memory_.reset();
      script_domain_ = "";
      break;

    case ScriptExecutor::CONTINUE:
      break;

    default:
      DVLOG(1) << "Unexpected value for at_end: " << result.at_end;
      break;
  }
  EnterState(AutofillAssistantState::PROMPT);
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
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
    parameters_.insert(
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
    should_fail_after_checking_scripts_ = true;
    MaybeSetInitialDetails();
    SetStatusMessage(l10n_util::GetStringFUTF8(
        IDS_AUTOFILL_ASSISTANT_LOADING, base::UTF8ToUTF16(initial_url.host())));
    SetProgress(kAutostartInitialProgress);
  }
}

void Controller::MaybeSetInitialDetails() {
  Details details;
  if (details.UpdateFromParameters(parameters_))
    SetDetails(details);
}

void Controller::Start(const GURL& initialUrl,
                       const std::map<std::string, std::string>& parameters) {
  if (state_ != AutofillAssistantState::INACTIVE) {
    NOTREACHED();
    return;
  }
  parameters_ = parameters;
  EnterState(AutofillAssistantState::STARTING);
  client_->ShowUI();
  if (IsCookieExperimentEnabled()) {
    GetWebController()->HasCookie(
        base::BindOnce(&Controller::OnGetCookie,
                       // WebController is owned by Controller.
                       base::Unretained(this), initialUrl));
  } else {
    FinishStart(initialUrl);
  }
}

AutofillAssistantState Controller::GetState() {
  return state_;
}

bool Controller::Terminate(Metrics::DropOutReason reason) {
  StopPeriodicScriptChecks();
  if (!will_shutdown_) {
    will_shutdown_ = true;
    GetUiController()->WillShutdown(reason);
  }
  if (script_tracker_ && !script_tracker_->Terminate()) {
    terminate_reason_ = reason;
    return false;
  }
  return true;
}

void Controller::OnScriptSelected(const std::string& script_path) {
  DCHECK(!script_path.empty());

  // This is a script selected from the UI, so it should disable autostart.
  allow_autostart_ = false;

  ExecuteScript(script_path);
}

void Controller::UpdateTouchableArea() {
  touchable_element_area()->UpdatePositions();
}

void Controller::OnUserInteractionInsideTouchableArea() {
  script_tracker()->CheckScripts(kPeriodicScriptCheckInterval);
  StartPeriodicScriptChecks();
}

std::string Controller::GetDebugContext() {
  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetKey("status", base::Value(status_message_));
  std::vector<base::Value> parameters_js;
  for (const auto& entry : parameters_) {
    base::Value parameter_js = base::Value(base::Value::Type::DICTIONARY);
    parameter_js.SetKey(entry.first, base::Value(entry.second));
    parameters_js.push_back(std::move(parameter_js));
  }
  dict.SetKey("parameters", base::Value(parameters_js));
  dict.SetKey("scripts", script_tracker()->GetDebugContext());

  if (details_)
    dict.SetKey("details", details_->GetDebugContext());

  std::string output_js;
  base::JSONWriter::Write(dict, &output_js);
  return output_js;
}

const PaymentRequestOptions* Controller::GetPaymentRequestOptions() const {
  return payment_request_options_.get();
}

void Controller::SetPaymentInformation(
    std::unique_ptr<PaymentInformation> payment_information) {
  if (!payment_request_options_)
    return;

  auto callback = std::move(payment_request_options_->callback);
  SetPaymentRequestOptions(nullptr);
  std::move(callback).Run(std::move(payment_information));
}

void Controller::GetTouchableArea(std::vector<RectF>* area) const {
  if (touchable_element_area_)
    touchable_element_area_->GetArea(area);
}

void Controller::OnFatalError(const std::string& error_message,
                              Metrics::DropOutReason reason) {
  LOG(ERROR) << "Autofill Assistant has encountered an error and is shutting "
                "down, reason="
             << reason;
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  StopPeriodicScriptChecks();
  SetStatusMessage(error_message);
  StopAndShutdown(reason);
}

void Controller::OnNoRunnableScriptsAnymore() {
  if (script_tracker()->running())
    return;

  // We're navigated to a page that has no scripts or the scripts have reached a
  // state from which they cannot recover through a DOM change.
  OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
               Metrics::NO_SCRIPTS);
  return;
}

void Controller::OnRunnableScriptsChanged(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker()->running() || state_ == AutofillAssistantState::STOPPED)
    return;

  if (!runnable_scripts.empty()) {
    should_fail_after_checking_scripts_ = false;
  }

  if (MaybeAutostartScript(runnable_scripts)) {
    return;
  }

  // Show the initial prompt if available.
  for (const auto& script : runnable_scripts) {
    // runnable_scripts is ordered by priority.
    if (!script.initial_prompt.empty()) {
      SetStatusMessage(script.initial_prompt);
      break;
    }
  }

  // Update the set of scripts in the UI.
  // TODO(crbug.com/806868): Surface type in proto instead of guessing it from
  // highlight flag.
  Chip::Type non_highlight_type = Chip::Type::CHIP_ASSISTIVE;
  for (const auto& script : runnable_scripts) {
    if (!script.autostart && !script.name.empty() && script.highlight) {
      non_highlight_type = Chip::Type::BUTTON_TEXT;
      break;
    }
  }

  auto chips = std::make_unique<std::vector<Chip>>();
  for (const auto& script : runnable_scripts) {
    if (!script.autostart && !script.name.empty()) {
      chips->emplace_back();
      chips->back().type = script.highlight ? Chip::Type::BUTTON_FILLED_BLUE
                                            : non_highlight_type;
      chips->back().text = script.name;
      chips->back().callback =
          base::BindOnce(&Controller::OnScriptSelected,
                         weak_ptr_factory_.GetWeakPtr(), script.path);
    }
  }

  if (allow_autostart_) {
    // Autostart was expected, but only non-autostartable scripts were found.
    //
    // TODO(crbug.com/806868): Consider the case where no non-autostartable
    // scripts were found.
    EnterState(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT);
  } else {
    EnterState(AutofillAssistantState::PROMPT);
  }
  SetChips(std::move(chips));
}

void Controller::DidAttachInterstitialPage() {
  client_->Shutdown(Metrics::INTERSTITIAL_PAGE);
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
  // The following types of navigations are allowed for the main frame, when
  // in PROMPT state:
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
  if (state_ == AutofillAssistantState::PROMPT &&
      navigation_handle->IsInMainFrame() &&
      web_contents()->GetLastCommittedURL().is_valid() &&
      !navigation_handle->WasServerRedirect() &&
      !navigation_handle->IsSameDocument() &&
      !navigation_handle->IsRendererInitiated()) {
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
                 Metrics::NAVIGATION);
  }
}

void Controller::DocumentAvailableInMainFrame() {
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::RenderProcessGone(base::TerminationStatus status) {
  client_->Shutdown(Metrics::RENDER_PROCESS_GONE);
}

void Controller::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (state_ != AutofillAssistantState::INACTIVE &&
      state_ != AutofillAssistantState::STOPPED) {
    client_->ShowUI();
  }
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
  auto iter = parameters_.find(kCookieExperimentName);
  return iter != parameters_.end() && iter->second == "1";
}

void Controller::OnTouchableAreaChanged(const std::vector<RectF>& areas) {
  GetUiController()->OnTouchableAreaChanged(areas);
}

void Controller::SetPaymentRequestOptions(
    std::unique_ptr<PaymentRequestOptions> options) {
  DCHECK(!options || options->callback);

  if (payment_request_options_ == nullptr && options == nullptr)
    return;

  payment_request_options_ = std::move(options);
  GetUiController()->OnPaymentRequestChanged(payment_request_options_.get());
}

ElementArea* Controller::touchable_element_area() {
  if (!touchable_element_area_) {
    touchable_element_area_ = std::make_unique<ElementArea>(this);
    touchable_element_area_->SetOnUpdate(base::BindRepeating(
        &Controller::OnTouchableAreaChanged, weak_ptr_factory_.GetWeakPtr()));
  }
  return touchable_element_area_.get();
}

ScriptTracker* Controller::script_tracker() {
  if (!script_tracker_) {
    script_tracker_ = std::make_unique<ScriptTracker>(/* delegate= */ this,
                                                      /* listener= */ this);
  }
  return script_tracker_.get();
}

}  // namespace autofill_assistant
