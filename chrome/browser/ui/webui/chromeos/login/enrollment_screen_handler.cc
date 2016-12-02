// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"

#include <algorithm>

#include "ash/common/system/chromeos/devicetype_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_screen.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace {

const char kJsScreenPath[] = "login.OAuthEnrollmentScreen";

// Enrollment step names.
const char kEnrollmentStepSignin[] = "signin";
const char kEnrollmentStepAdJoin[] = "ad-join";
const char kEnrollmentStepSuccess[] = "success";
const char kEnrollmentStepWorking[] = "working";

// Enrollment mode constants used in the UI. This needs to be kept in sync with
// oobe_screen_oauth_enrollment.js.
const char kEnrollmentModeUIForced[] = "forced";
const char kEnrollmentModeUIManual[] = "manual";
const char kEnrollmentModeUIRecovery[] = "recovery";

// Converts |mode| to a mode identifier for the UI.
std::string EnrollmentModeToUIMode(policy::EnrollmentConfig::Mode mode) {
  switch (mode) {
    case policy::EnrollmentConfig::MODE_NONE:
      break;
    case policy::EnrollmentConfig::MODE_MANUAL:
    case policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
    case policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED:
    case policy::EnrollmentConfig::MODE_SERVER_ADVERTISED:
    case policy::EnrollmentConfig::MODE_ATTESTATION:
      return kEnrollmentModeUIManual;
    case policy::EnrollmentConfig::MODE_LOCAL_FORCED:
    case policy::EnrollmentConfig::MODE_SERVER_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_FORCED:
      return kEnrollmentModeUIForced;
    case policy::EnrollmentConfig::MODE_RECOVERY:
      return kEnrollmentModeUIRecovery;
  }

  NOTREACHED() << "Bad enrollment mode " << mode;
  return kEnrollmentModeUIManual;
}

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network)
    return std::string();
  return network->name();
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
         reason == NetworkError::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == NetworkError::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == NetworkError::ERROR_REASON_PROXY_CONNECTION_FAILED;
}


// Returns the enterprise domain after enrollment, or an empty string.
std::string GetEnterpriseDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetEnterpriseDomain();
}

// Returns file descriptor of a pipe, open for reading. Pipe keeps user
// password, which can be read from the returned descriptor.
base::ScopedFD GetPasswordReadPipe(const std::string& password) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  int pipe_fds[2];
  if (!base::CreateLocalNonBlockingPipe(pipe_fds)) {
    LOG(ERROR) << "Failed to create pipe";
    return base::ScopedFD();
  }
  base::ScopedFD pipe_read_end(pipe_fds[0]);
  base::ScopedFD pipe_write_end(pipe_fds[1]);

  if (!base::WriteFileDescriptor(pipe_write_end.get(),
                                 password.c_str(),
                                 password.size())) {
    LOG(ERROR) << "Failed to write to pipe";
    return base::ScopedFD();
  }
  return pipe_read_end;
}

}  // namespace

// EnrollmentScreenHandler, public ------------------------------

EnrollmentScreenHandler::EnrollmentScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    NetworkErrorModel* network_error_model)
    : BaseScreenHandler(kJsScreenPath),
      controller_(NULL),
      show_on_init_(false),
      first_show_(true),
      observe_network_failure_(false),
      network_state_informer_(network_state_informer),
      network_error_model_(network_error_model),
      histogram_helper_(new ErrorScreensHistogramHelper("Enrollment")),
      weak_ptr_factory_(this) {
  set_async_assets_load_id(
      GetOobeScreenName(OobeScreen::SCREEN_OOBE_ENROLLMENT));
  DCHECK(network_state_informer_.get());
  DCHECK(network_error_model_);
  network_state_informer_->AddObserver(this);
}

EnrollmentScreenHandler::~EnrollmentScreenHandler() {
  network_state_informer_->RemoveObserver(this);
}

// EnrollmentScreenHandler, WebUIMessageHandler implementation --

void EnrollmentScreenHandler::RegisterMessages() {
  AddCallback("toggleFakeEnrollment",
              &EnrollmentScreenHandler::HandleToggleFakeEnrollment);
  AddCallback("oauthEnrollClose",
              &EnrollmentScreenHandler::HandleClose);
  AddCallback("oauthEnrollCompleteLogin",
              &EnrollmentScreenHandler::HandleCompleteLogin);
  AddCallback("oauthEnrollAdCompleteLogin",
              &EnrollmentScreenHandler::HandleAdCompleteLogin);
  AddCallback("oauthEnrollRetry",
              &EnrollmentScreenHandler::HandleRetry);
  AddCallback("frameLoadingCompleted",
              &EnrollmentScreenHandler::HandleFrameLoadingCompleted);
  AddCallback("oauthEnrollAttributes",
              &EnrollmentScreenHandler::HandleDeviceAttributesProvided);
  AddCallback("oauthEnrollOnLearnMore",
              &EnrollmentScreenHandler::HandleOnLearnMore);
}

// EnrollmentScreenHandler
//      EnrollmentScreenActor implementation -----------------------------------

void EnrollmentScreenHandler::SetParameters(
    Controller* controller,
    const policy::EnrollmentConfig& config) {
  CHECK(config.should_enroll());
  controller_ = controller;
  config_ = config;
}

void EnrollmentScreenHandler::PrepareToShow() {
}

void EnrollmentScreenHandler::Show() {
  if (!page_is_ready())
    show_on_init_ = true;
  else
    DoShow();
}

void EnrollmentScreenHandler::Hide() {
}

void EnrollmentScreenHandler::ShowSigninScreen() {
  observe_network_failure_ = true;
  ShowStep(kEnrollmentStepSignin);
}

void EnrollmentScreenHandler::ShowAdJoin() {
  observe_network_failure_ = false;
  ShowStep(kEnrollmentStepAdJoin);
}

void EnrollmentScreenHandler::ShowAttributePromptScreen(
    const std::string& asset_id,
    const std::string& location) {
  CallJS("showAttributePromptStep", asset_id, location);
}

void EnrollmentScreenHandler::ShowEnrollmentSpinnerScreen() {
  ShowStep(kEnrollmentStepWorking);
}

void EnrollmentScreenHandler::ShowAttestationBasedEnrollmentSuccessScreen(
    const std::string& enterprise_domain) {
  CallJS("showAttestationBasedEnrollmentSuccess", ash::GetChromeOSDeviceName(),
         enterprise_domain);
}

void EnrollmentScreenHandler::ShowAuthError(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::WEB_LOGIN_REQUIRED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR, false);
      return;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, true);
      return;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_NETWORK_ERROR, true);
      return;
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED_DEPRECATED:
    case GoogleServiceAuthError::NUM_STATES:
      break;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::ShowOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  switch (error) {
    case EnterpriseEnrollmentHelper::OTHER_ERROR_DOMAIN_MISMATCH:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
      return;
    case EnterpriseEnrollmentHelper::OTHER_ERROR_FATAL:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_ENROLLMENT_ERROR, true);
      return;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::ShowEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.status()) {
    case policy::EnrollmentStatus::SUCCESS:
      if (config_.is_mode_attestation())
        ShowAttestationBasedEnrollmentSuccessScreen(GetEnterpriseDomain());
      else
        ShowStep(kEnrollmentStepSuccess);
      return;
    case policy::EnrollmentStatus::NO_STATE_KEYS:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_NO_STATE_KEYS, false);
      return;
    case policy::EnrollmentStatus::REGISTRATION_FAILED:
      // Some special cases for generating a nicer message that's more helpful.
      switch (status.client_status()) {
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DEPROVISIONED_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, true);
          break;
        default:
          ShowErrorMessage(
              l10n_util::GetStringFUTF8(
                  IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_FAILED,
                  policy::FormatDeviceManagementStatus(status.client_status())),
              true);
      }
      return;
    case policy::EnrollmentStatus::ROBOT_AUTH_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_AUTH_FETCH_FAILED, true);
      return;
    case policy::EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_FETCH_FAILED, true);
      return;
    case policy::EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_STORE_FAILED, true);
      return;
    case policy::EnrollmentStatus::REGISTRATION_BAD_MODE:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_BAD_MODE, false);
      return;
    case policy::EnrollmentStatus::REGISTRATION_CERT_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_CERT_FETCH_FAILED,
                true);
      return;
    case policy::EnrollmentStatus::POLICY_FETCH_FAILED:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_POLICY_FETCH_FAILED,
              policy::FormatDeviceManagementStatus(status.client_status())),
          true);
      return;
    case policy::EnrollmentStatus::VALIDATION_FAILED:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_VALIDATION_FAILED,
              policy::FormatValidationStatus(status.validation_status())),
          true);
      return;
    case policy::EnrollmentStatus::LOCK_ERROR:
      switch (status.lock_status()) {
        case InstallAttributes::LOCK_SUCCESS:
        case InstallAttributes::LOCK_NOT_READY:
          // LOCK_SUCCESS is in contradiction of STATUS_LOCK_ERROR.
          // LOCK_NOT_READY is transient, if retries are given up, LOCK_TIMEOUT
          // is reported instead.  This piece of code is unreached.
          LOG(FATAL) << "Invalid lock status.";
          return;
        case InstallAttributes::LOCK_TIMEOUT:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_TIMEOUT, false);
          return;
        case InstallAttributes::LOCK_BACKEND_INVALID:
        case InstallAttributes::LOCK_ALREADY_LOCKED:
        case InstallAttributes::LOCK_SET_ERROR:
        case InstallAttributes::LOCK_FINALIZE_ERROR:
        case InstallAttributes::LOCK_READBACK_ERROR:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_ERROR, false);
          return;
        case InstallAttributes::LOCK_WRONG_DOMAIN:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
          return;
        case InstallAttributes::LOCK_WRONG_MODE:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_MODE, true);
          return;
      }
      NOTREACHED();
      return;
    case policy::EnrollmentStatus::STORE_ERROR:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_STORE_ERROR,
              policy::FormatStoreStatus(status.store_status(),
                                        status.validation_status())),
          true);
      return;
    case policy::EnrollmentStatus::ATTRIBUTE_UPDATE_FAILED:
      ShowErrorForDevice(IDS_ENTERPRISE_ENROLLMENT_ATTRIBUTE_ERROR, false);
      return;
    case policy::EnrollmentStatus::NO_MACHINE_IDENTIFICATION:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_NO_MACHINE_IDENTIFICATION,
                false);
      return;
    case policy::EnrollmentStatus::ACTIVE_DIRECTORY_POLICY_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ERROR_ACTIVE_DIRECTORY_POLICY_FETCH,
                false);
      return;
  }
  NOTREACHED();
}

// EnrollmentScreenHandler BaseScreenHandler implementation -----

void EnrollmentScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EnrollmentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("oauthEnrollScreenTitle",
               IDS_ENTERPRISE_ENROLLMENT_SCREEN_TITLE);
  builder->Add("oauthEnrollRetry", IDS_ENTERPRISE_ENROLLMENT_RETRY);
  builder->Add("oauthEnrollDone", IDS_ENTERPRISE_ENROLLMENT_DONE);
  builder->Add("oauthEnrollNextBtn", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("oauthEnrollSkip", IDS_ENTERPRISE_ENROLLMENT_SKIP);
  builder->AddF("oauthEnrollSuccess", IDS_ENTERPRISE_ENROLLMENT_SUCCESS,
                ash::GetChromeOSDeviceName());
  builder->Add("oauthEnrollDeviceInformation",
               IDS_ENTERPRISE_ENROLLMENT_DEVICE_INFORMATION);
  builder->Add("oauthEnrollExplainAttributeLink",
               IDS_ENTERPRISE_ENROLLMENT_EXPLAIN_ATTRIBUTE_LINK);
  builder->Add("oauthEnrollAttributeExplanation",
               IDS_ENTERPRISE_ENROLLMENT_ATTRIBUTE_EXPLANATION);
  builder->Add("oauthEnrollAssetIdLabel",
               IDS_ENTERPRISE_ENROLLMENT_ASSET_ID_LABEL);
  builder->Add("oauthEnrollLocationLabel",
               IDS_ENTERPRISE_ENROLLMENT_LOCATION_LABEL);
  builder->Add("oauthEnrollWorking", IDS_ENTERPRISE_ENROLLMENT_WORKING_MESSAGE);
  // Do not use AddF for this string as it will be rendered by the JS code.
  builder->Add("oauthEnrollAbeSuccess", IDS_ENTERPRISE_ENROLLMENT_ABE_SUCCESS);
  builder->Add("oauthEnrollAdMachineNameInput",
               IDS_AD_MACHINE_NAME_INPUT_LABEL);
  builder->Add("oauthEnrollAdDomainJoinWelcomeMessage",
               IDS_AD_DOMAIN_JOIN_WELCOME_MESSAGE);
  builder->Add("adLoginUser", IDS_AD_LOGIN_USER);
  builder->Add("adLoginPassword", IDS_AD_LOGIN_PASSWORD);
}

bool EnrollmentScreenHandler::IsOnEnrollmentScreen() const {
  return (GetCurrentScreen() == OobeScreen::SCREEN_OOBE_ENROLLMENT);
}

bool EnrollmentScreenHandler::IsEnrollmentScreenHiddenByError() const {
  return (GetCurrentScreen() == OobeScreen::SCREEN_ERROR_MESSAGE &&
          network_error_model_->GetParentScreen() ==
              OobeScreen::SCREEN_OOBE_ENROLLMENT);
}

void EnrollmentScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  UpdateStateInternal(reason, false);
}

// TODO(rsorokin): This function is mostly copied from SigninScreenHandler and
// should be refactored in the future.
void EnrollmentScreenHandler::UpdateStateInternal(
    NetworkError::ErrorReason reason,
    bool force_update) {
  if (!force_update && !IsOnEnrollmentScreen() &&
      !IsEnrollmentScreenHiddenByError()) {
    return;
  }

  if (!force_update && !observe_network_failure_)
    return;

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const bool is_online = (state == NetworkStateInformer::ONLINE);
  const bool is_behind_captive_portal =
      (state == NetworkStateInformer::CAPTIVE_PORTAL);
  const bool is_frame_error = reason == NetworkError::ERROR_REASON_FRAME_ERROR;

  LOG(WARNING) << "EnrollmentScreenHandler::UpdateState(): "
               << "state=" << NetworkStateInformer::StatusString(state) << ", "
               << "reason=" << NetworkError::ErrorReasonString(reason);

  if (is_online || !is_behind_captive_portal)
    network_error_model_->HideCaptivePortal();

  if (is_frame_error) {
    LOG(WARNING) << "Retry page load";
    // TODO(rsorokin): Too many consecutive reloads.
    CallJS("doReload");
  }

  if (!is_online || is_frame_error)
    SetupAndShowOfflineMessage(state, reason);
  else
    HideOfflineMessage(state, reason);
}

void EnrollmentScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = IsProxyError(state, reason);
  const bool is_frame_error = reason == NetworkError::ERROR_REASON_FRAME_ERROR;

  if (is_proxy_error) {
    network_error_model_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                        std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsOnEnrollmentScreen() || (network_error_model_->GetErrorState() !=
                                   NetworkError::ERROR_STATE_PORTAL)) {
      network_error_model_->FixCaptivePortal();
    }
    const std::string network_name = GetNetworkName(network_path);
    network_error_model_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                        network_name);
  } else if (is_frame_error) {
    network_error_model_->SetErrorState(
        NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT, std::string());
  } else {
    network_error_model_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                        std::string());
  }

  if (GetCurrentScreen() != OobeScreen::SCREEN_ERROR_MESSAGE) {
    const std::string network_type = network_state_informer_->network_type();
    network_error_model_->SetUIState(NetworkError::UI_STATE_SIGNIN);
    network_error_model_->SetParentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
    network_error_model_->SetHideCallback(base::Bind(
        &EnrollmentScreenHandler::DoShow, weak_ptr_factory_.GetWeakPtr()));
    network_error_model_->Show();
    histogram_helper_->OnErrorShow(network_error_model_->GetErrorState());
  }
}

void EnrollmentScreenHandler::HideOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  if (IsEnrollmentScreenHiddenByError())
    network_error_model_->Hide();
  histogram_helper_->OnErrorHide();
}

// EnrollmentScreenHandler, private -----------------------------
void EnrollmentScreenHandler::HandleToggleFakeEnrollment() {
  policy::PolicyOAuth2TokenFetcher::UseFakeTokensForTesting();
}

void EnrollmentScreenHandler::HandleClose(const std::string& reason) {
  DCHECK(controller_);

  if (reason == "cancel")
    controller_->OnCancel();
  else if (reason == "done")
    controller_->OnConfirmationClosed();
  else
    NOTREACHED();
}

void EnrollmentScreenHandler::HandleCompleteLogin(
    const std::string& user,
    const std::string& auth_code) {
  observe_network_failure_ = false;
  DCHECK(controller_);
  controller_->OnLoginDone(gaia::SanitizeEmail(user), auth_code);
}

void EnrollmentScreenHandler::HandleAdCompleteLogin(
    const std::string& machine_name,
    const std::string& user_name,
    const std::string& password) {
  observe_network_failure_ = false;
  DCHECK(controller_);
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&GetPasswordReadPipe, password),
      base::Bind(&EnrollmentScreenHandler::OnPasswordPipeReady,
                 weak_ptr_factory_.GetWeakPtr(), machine_name, user_name));
}

void EnrollmentScreenHandler::OnPasswordPipeReady(
    const std::string& machine_name,
    const std::string& user_name,
    base::ScopedFD password_fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!password_fd.is_valid()) {
    LOG(ERROR) << "Got invalid password_fd";
    return;
  }
  chromeos::AuthPolicyClient* client =
      chromeos::DBusThreadManager::Get()->GetAuthPolicyClient();

  client->JoinAdDomain(machine_name,
                       user_name,
                       password_fd.get(),
                       base::Bind(&EnrollmentScreenHandler::HandleAdDomainJoin,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  machine_name,
                                  user_name));
}

void EnrollmentScreenHandler::HandleAdDomainJoin(
    const std::string& machine_name,
    const std::string& user_name,
    int code) {
  if (code == 0) {
    controller_->OnAdJoined(gaia::ExtractDomainName(user_name));
    return;
  }
  // TODO(rsorokin): Add passing/displaying error codes. (see crbug.com/659984)
  CallJS("invalidateAd", machine_name, user_name);
}

void EnrollmentScreenHandler::HandleRetry() {
  DCHECK(controller_);
  controller_->OnRetry();
}

void EnrollmentScreenHandler::HandleFrameLoadingCompleted() {
  if (network_state_informer_->state() != NetworkStateInformer::ONLINE)
    return;

  UpdateState(NetworkError::ERROR_REASON_UPDATE);
}

void EnrollmentScreenHandler::HandleDeviceAttributesProvided(
    const std::string& asset_id,
    const std::string& location) {
  controller_->OnDeviceAttributeProvided(asset_id, location);
}

void EnrollmentScreenHandler::HandleOnLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_DEVICE_ATTRIBUTES);
}

void EnrollmentScreenHandler::ShowStep(const char* step) {
  CallJS("showStep", std::string(step));
}

void EnrollmentScreenHandler::ShowError(int message_id, bool retry) {
  ShowErrorMessage(l10n_util::GetStringUTF8(message_id), retry);
}

void EnrollmentScreenHandler::ShowErrorForDevice(int message_id, bool retry) {
  ShowErrorMessage(
      l10n_util::GetStringFUTF8(message_id, ash::GetChromeOSDeviceName()),
      retry);
}

void EnrollmentScreenHandler::ShowErrorMessage(const std::string& message,
                                               bool retry) {
  CallJS("showError", message, retry);
}

void EnrollmentScreenHandler::DoShow() {
  base::DictionaryValue screen_data;
  screen_data.SetString("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  screen_data.SetString("clientId",
                        GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  screen_data.SetString("enrollment_mode",
                        EnrollmentModeToUIMode(config_.mode));
  screen_data.SetBoolean("attestationBased", config_.is_mode_attestation());
  screen_data.SetString("management_domain", config_.management_domain);

  const bool cfm = g_browser_process->platform_part()
                       ->browser_policy_connector_chromeos()
                       ->GetDeviceCloudPolicyManager()
                       ->IsRemoraRequisition();
  screen_data.SetString("flow", cfm ? "cfm" : "enterprise");

  ShowScreenWithData(OobeScreen::SCREEN_OOBE_ENROLLMENT, &screen_data);
  if (first_show_) {
    first_show_ = false;
    UpdateStateInternal(NetworkError::ERROR_REASON_UPDATE, true);
  }
  histogram_helper_->OnScreenShow();
}

}  // namespace chromeos
