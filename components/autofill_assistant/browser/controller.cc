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
#include "base/time/tick_clock.h"
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
// This limit does not apply when in autostart mode.
static constexpr int kPeriodicScriptCheckCount = 10;

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

Controller::Controller(content::WebContents* web_contents,
                       Client* client,
                       const base::TickClock* tick_clock)
    : content::WebContentsObserver(web_contents),
      client_(client),
      tick_clock_(tick_clock),
      weak_ptr_factory_(this) {}

Controller::~Controller() = default;

const GURL& Controller::GetCurrentURL() {
  const GURL& last_committed = web_contents()->GetLastCommittedURL();
  if (!last_committed.is_empty())
    return last_committed;

  return initial_url_;
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

void Controller::SetInfoBox(const InfoBox& info_box) {
  if (!info_box_) {
    info_box_ = std::make_unique<InfoBox>();
  }
  *info_box_ = info_box;
  GetUiController()->OnInfoBoxChanged(info_box_.get());
}

void Controller::ClearInfoBox() {
  info_box_.reset();
  GetUiController()->OnInfoBoxChanged(nullptr);
}

const InfoBox* Controller::GetInfoBox() const {
  return info_box_.get();
}

void Controller::SetProgress(int progress) {
  // Progress can only increase.
  if (progress_ >= progress)
    return;

  progress_ = progress;
  GetUiController()->OnProgressChanged(progress);
}

void Controller::SetProgressVisible(bool visible) {
  if (progress_visible_ == visible)
    return;

  progress_visible_ = visible;
  GetUiController()->OnProgressVisibilityChanged(visible);
}

bool Controller::GetProgressVisible() const {
  return progress_visible_;
}

const std::vector<Chip>& Controller::GetSuggestions() const {
  static const base::NoDestructor<std::vector<Chip>> no_suggestions_;
  return suggestions_ ? *suggestions_ : *no_suggestions_;
}

const std::vector<Chip>& Controller::GetActions() const {
  static const base::NoDestructor<std::vector<Chip>> no_actions_;
  return actions_ ? *actions_ : *no_actions_;
}

void Controller::SetChips(std::unique_ptr<std::vector<Chip>> chips) {
  // We split the chips into suggestions and actions, that are displayed in
  // different carousels.
  actions_.reset();
  suggestions_.reset();

  if (chips && !chips->empty()) {
    for (auto iter = chips->begin(); iter != chips->end(); iter++) {
      if (iter->type == SUGGESTION) {
        if (!suggestions_) {
          suggestions_ = std::make_unique<std::vector<Chip>>();
        }

        suggestions_->emplace_back(std::move(*iter));
      } else {
        if (!actions_) {
          actions_ = std::make_unique<std::vector<Chip>>();
        }

        actions_->emplace_back(std::move(*iter));
      }
    }
  }

  GetUiController()->OnSuggestionsChanged(GetSuggestions());
  GetUiController()->OnActionsChanged(GetActions());
}

void Controller::SelectSuggestion(int index) {
  SelectChip(suggestions_.get(), index);
}

void Controller::SelectAction(int index) {
  SelectChip(actions_.get(), index);
}

void Controller::SelectChip(std::vector<Chip>* chips, int chip_index) {
  if (!chips || chip_index < 0 ||
      static_cast<size_t>(chip_index) >= chips->size()) {
    NOTREACHED() << "Invalid chip index: " << chip_index;
    return;
  }

  if (!(*chips)[chip_index].callback) {
    return;
  }

  auto callback = std::move((*chips)[chip_index].callback);
  SetChips(nullptr);
  std::move(callback).Run();
}

void Controller::StopAndShutdown(Metrics::DropOutReason reason) {
  ClearInfoBox();
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
  DVLOG(2) << __func__ << ": " << state_ << " -> " << state;

  state_ = state;
  GetUiController()->OnStateChanged(state);
}

void Controller::SetWebControllerAndServiceForTest(
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<Service> service) {
  web_controller_ = std::move(web_controller);
  service_ = std::move(service);
}

void Controller::GetOrCheckScripts() {
  if (!started_ || script_tracker()->running()) {
    return;
  }

  const GURL& url = GetCurrentURL();
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
  if (periodic_script_check_count_ > 0) {
    periodic_script_check_count_--;
  }

  if (periodic_script_check_count_ <= 0 && !allow_autostart_) {
    DCHECK_EQ(0, periodic_script_check_count_);
    periodic_script_check_scheduled_ = false;
    return;
  }

  if (allow_autostart_ && !autostart_timeout_script_path_.empty() &&
      tick_clock_->NowTicks() >= absolute_autostart_timeout_) {
    DVLOG(1) << __func__ << " giving up waiting on autostart.";
    std::string script_path = autostart_timeout_script_path_;
    autostart_timeout_script_path_.clear();
    periodic_script_check_scheduled_ = false;
    ExecuteScript(script_path, state_);
    return;
  }

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
    DVLOG(1) << "Failed to get assistant scripts for " << script_domain_;
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::GET_SCRIPTS_FAILED);
    return;
  }

  SupportsScriptResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    DVLOG(2) << __func__ << " from " << script_domain_ << " returned "
             << "unparseable response";
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::GET_SCRIPTS_UNPARSABLE);
    return;
  }

  std::vector<std::unique_ptr<Script>> scripts;
  for (const auto& script_proto : response_proto.scripts()) {
    ProtocolUtils::AddScript(script_proto, &scripts);
  }
  if (scripts.empty()) {
    OnNoRunnableScripts();
    return;
  }

  if (allow_autostart_) {
    autostart_timeout_script_path_ =
        response_proto.script_timeout_error().script_path();
    autostart_timeout_ = base::TimeDelta::FromMilliseconds(
        response_proto.script_timeout_error().timeout_ms());
    absolute_autostart_timeout_ = tick_clock_->NowTicks() + autostart_timeout_;
  }

  DVLOG(2) << __func__ << " from " << script_domain_ << " returned "
           << scripts.size() << " scripts";
  script_tracker()->SetScripts(std::move(scripts));
  script_tracker()->CheckScripts(kPeriodicScriptCheckInterval);
  StartPeriodicScriptChecks();
}

void Controller::ExecuteScript(const std::string& script_path,
                               AutofillAssistantState end_state) {
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
      script_path,
      base::BindOnce(&Controller::OnScriptExecuted,
                     // script_tracker_ is owned by Controller.
                     base::Unretained(this), script_path, end_state));
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  AutofillAssistantState end_state,
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
  EnterState(end_state);
  GetOrCheckScripts();
}

bool Controller::MaybeAutostartScript(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // We want to go through all runnable autostart interrupts first, one at a
  // time. To do that, always run highest priority autostartable interrupt from
  // runnable_script, which is ordered by priority.
  for (const auto& script : runnable_scripts) {
    if (script.autostart && script.interrupt) {
      std::string script_path = script.path;
      ExecuteScript(script_path, state_);
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
      DisableAutostart();
      ExecuteScript(autostart_path, AutofillAssistantState::PROMPT);
      return true;
    }
  }
  return false;
}

void Controller::DisableAutostart() {
  allow_autostart_ = false;
  autostart_timeout_script_path_.clear();
}

void Controller::OnGetCookie(bool has_cookie) {
  if (has_cookie) {
    // This code is only active with the experiment parameter.
    parameters_.insert(
        std::make_pair(kWebsiteVisitedBeforeParameterName, kTrueValue));
    OnSetCookie(has_cookie);
    return;
  }
  GetWebController()->SetCookie(
      initial_url_.host(),
      base::BindOnce(&Controller::OnSetCookie,
                     // WebController is owned by Controller.
                     base::Unretained(this)));
}

void Controller::OnSetCookie(bool result) {
  DCHECK(result) << "Setting cookie failed";
  FinishStart();
}

void Controller::FinishStart() {
  started_ = true;
  if (allow_autostart_) {
    MaybeSetInitialDetails();
    SetStatusMessage(
        l10n_util::GetStringFUTF8(IDS_AUTOFILL_ASSISTANT_LOADING,
                                  base::UTF8ToUTF16(initial_url_.host())));
    SetProgress(kAutostartInitialProgress);
  }
  GetOrCheckScripts();
}

void Controller::MaybeSetInitialDetails() {
  Details details;
  if (details.UpdateFromParameters(parameters_))
    SetDetails(details);
}

bool Controller::NeedsUI() const {
  return state_ != AutofillAssistantState::INACTIVE &&
         state_ != AutofillAssistantState::STOPPED;
}

void Controller::Start(const GURL& initial_url,
                       const std::map<std::string, std::string>& parameters) {
  if (state_ != AutofillAssistantState::INACTIVE) {
    NOTREACHED();
    return;
  }
  parameters_ = parameters;
  initial_url_ = initial_url;
  EnterState(AutofillAssistantState::STARTING);
  client_->ShowUI();
  if (IsCookieExperimentEnabled()) {
    GetWebController()->HasCookie(
        base::BindOnce(&Controller::OnGetCookie,
                       // WebController is owned by Controller.
                       base::Unretained(this)));
  } else {
    FinishStart();
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
  ExecuteScript(script_path, AutofillAssistantState::PROMPT);
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

void Controller::OnPaymentRequestContinueButtonClicked() {
  if (!payment_request_options_ || !payment_request_info_)
    return;

  auto callback = std::move(payment_request_options_->callback);
  auto payment_request_info = std::move(payment_request_info_);

  // TODO(crbug.com/806868): succeed is currently always true, but we might want
  // to set it to false and propagate the result to GetPaymentInformationAction
  // when the user clicks "Cancel" during that action.
  payment_request_info->succeed = true;

  SetPaymentRequestOptions(nullptr);
  std::move(callback).Run(std::move(payment_request_info));
}

void Controller::SetShippingAddress(
    std::unique_ptr<autofill::AutofillProfile> address) {
  if (!payment_request_info_)
    return;

  payment_request_info_->shipping_address = std::move(address);
  UpdatePaymentRequestActions();
}

void Controller::SetBillingAddress(
    std::unique_ptr<autofill::AutofillProfile> address) {
  if (!payment_request_info_)
    return;

  payment_request_info_->billing_address = std::move(address);
  UpdatePaymentRequestActions();
}

void Controller::SetContactInfo(std::string name,
                                std::string phone,
                                std::string email) {
  if (!payment_request_info_)
    return;

  payment_request_info_->payer_name = name;
  payment_request_info_->payer_phone = phone;
  payment_request_info_->payer_email = email;
  UpdatePaymentRequestActions();
}

void Controller::SetCreditCard(std::unique_ptr<autofill::CreditCard> card) {
  if (!payment_request_info_)
    return;

  payment_request_info_->card = std::move(card);
  UpdatePaymentRequestActions();
}

void Controller::SetTermsAndConditions(
    TermsAndConditionsState terms_and_conditions) {
  if (!payment_request_info_)
    return;

  payment_request_info_->terms_and_conditions = terms_and_conditions;
  UpdatePaymentRequestActions();
}

void Controller::UpdatePaymentRequestActions() {
  // TODO(crbug.com/806868): This method uses #SetChips(), which means that
  // updating the PR actions will also clear the suggestions. We should update
  // the actions only if there are use cases of PR + suggestions.
  if (!payment_request_options_ || !payment_request_info_) {
    return;
  }

  bool contact_info_ok = (!payment_request_options_->request_payer_name ||
                          !payment_request_info_->payer_name.empty()) &&
                         (!payment_request_options_->request_payer_email ||
                          !payment_request_info_->payer_email.empty()) &&
                         (!payment_request_options_->request_payer_phone ||
                          !payment_request_info_->payer_phone.empty());

  bool shipping_address_ok = !payment_request_options_->request_shipping ||
                             payment_request_info_->shipping_address;

  bool terms_ok = payment_request_info_->terms_and_conditions != NOT_SELECTED;

  bool continue_button_enabled = contact_info_ok && shipping_address_ok &&
                                 payment_request_info_->card && terms_ok;

  auto chips = std::make_unique<std::vector<Chip>>();
  chips->emplace_back();
  chips->back().text =
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM);
  chips->back().type = HIGHLIGHTED_ACTION;
  chips->back().disabled = !continue_button_enabled;
  if (continue_button_enabled) {
    chips->back().callback =
        base::BindOnce(&Controller::OnPaymentRequestContinueButtonClicked,
                       weak_ptr_factory_.GetWeakPtr());
  }

  SetChips(std::move(chips));
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

void Controller::OnNoRunnableScripts() {
  if (script_tracker()->running())
    return;

  if (state_ == AutofillAssistantState::STARTING) {
    // We're still waiting for the set of initial scripts, but either didn't get
    // any scripts or didn't get scripts that could possibly become runnable
    // with a DOM change.
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::NO_INITIAL_SCRIPTS);
    return;
  }

  // We're navigated to a page that has no scripts or the scripts have reached a
  // state from which they cannot recover through a DOM change.
  OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
               Metrics::NO_SCRIPTS);
}

void Controller::OnRunnableScriptsChanged(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker()->running() || state_ == AutofillAssistantState::STOPPED)
    return;

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
  auto chips = std::make_unique<std::vector<Chip>>();
  for (const auto& script : runnable_scripts) {
    if (!script.autostart && !script.name.empty()) {
      chips->emplace_back();
      chips->back().text = script.name;
      chips->back().type = script.chip_type;
      chips->back().callback =
          base::BindOnce(&Controller::OnScriptSelected,
                         weak_ptr_factory_.GetWeakPtr(), script.path);
    }
  }
  SetDefaultChipType(chips.get());

  if (chips->empty() && state_ == AutofillAssistantState::STARTING) {
    // Continue waiting
    return;
  }

  if (allow_autostart_ ||
      state_ == AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT) {
    // Autostart was expected, but only non-autostartable scripts were found.
    DisableAutostart();
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
  GetOrCheckScripts();
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
  GetOrCheckScripts();
}

void Controller::RenderProcessGone(base::TerminationStatus status) {
  client_->Shutdown(Metrics::RENDER_PROCESS_GONE);
}

void Controller::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (NeedsUI()) {
    client_->ShowUI();
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

  if (options) {
    payment_request_info_ = std::make_unique<PaymentInformation>();
  }

  payment_request_options_ = std::move(options);
  UpdatePaymentRequestActions();
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
