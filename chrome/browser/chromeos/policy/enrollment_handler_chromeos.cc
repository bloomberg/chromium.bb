// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/enrollment_handler_chromeos.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Retry for InstallAttrs initialization every 500ms.
const int kLockRetryIntervalMs = 500;
// Maximum time to retry InstallAttrs initialization before we give up.
const int kLockRetryTimeoutMs = 10 * 60 * 1000;  // 10 minutes.

em::DeviceRegisterRequest::Flavor EnrollmentModeToRegistrationFlavor(
    policy::EnrollmentConfig::Mode mode) {
  switch (mode) {
    case policy::EnrollmentConfig::MODE_NONE:
      break;
    case policy::EnrollmentConfig::MODE_MANUAL:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL;
    case policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL_RENEW;
    case policy::EnrollmentConfig::MODE_LOCAL_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_LOCAL_FORCED;
    case policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_LOCAL_ADVERTISED;
    case policy::EnrollmentConfig::MODE_SERVER_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_SERVER_FORCED;
    case policy::EnrollmentConfig::MODE_SERVER_ADVERTISED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_SERVER_ADVERTISED;
    case policy::EnrollmentConfig::MODE_RECOVERY:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY;
  }

  NOTREACHED() << "Bad enrollment mode: " << mode;
  return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL;
}

}  // namespace

EnrollmentHandlerChromeOS::EnrollmentHandlerChromeOS(
    DeviceCloudPolicyStoreChromeOS* store,
    EnterpriseInstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    chromeos::OwnerSettingsServiceChromeOS* owner_settings_service,
    scoped_ptr<CloudPolicyClient> client,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const EnrollmentConfig& enrollment_config,
    const std::string& auth_token,
    const std::string& client_id,
    const std::string& requisition,
    const AllowedDeviceModes& allowed_device_modes,
    ManagementMode management_mode,
    const EnrollmentCallback& completion_callback)
    : store_(store),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      owner_settings_service_(owner_settings_service),
      client_(client.Pass()),
      background_task_runner_(background_task_runner),
      enrollment_config_(enrollment_config),
      auth_token_(auth_token),
      client_id_(client_id),
      requisition_(requisition),
      allowed_device_modes_(allowed_device_modes),
      management_mode_(management_mode),
      completion_callback_(completion_callback),
      device_mode_(DEVICE_MODE_NOT_SET),
      skip_robot_auth_(false),
      enrollment_step_(STEP_PENDING),
      lockbox_init_duration_(0),
      weak_ptr_factory_(this) {
  CHECK(!client_->is_registered());
  CHECK_EQ(DM_STATUS_SUCCESS, client_->status());
  CHECK(management_mode_ == MANAGEMENT_MODE_ENTERPRISE_MANAGED ||
        management_mode_ == MANAGEMENT_MODE_CONSUMER_MANAGED);
  store_->AddObserver(this);
  client_->AddObserver(this);
  client_->AddPolicyTypeToFetch(dm_protocol::kChromeDevicePolicyType,
                                std::string());
}

EnrollmentHandlerChromeOS::~EnrollmentHandlerChromeOS() {
  Stop();
  store_->RemoveObserver(this);
}

void EnrollmentHandlerChromeOS::StartEnrollment() {
  CHECK_EQ(STEP_PENDING, enrollment_step_);
  enrollment_step_ = STEP_STATE_KEYS;
  state_keys_broker_->RequestStateKeys(
      base::Bind(&EnrollmentHandlerChromeOS::HandleStateKeysResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

scoped_ptr<CloudPolicyClient> EnrollmentHandlerChromeOS::ReleaseClient() {
  Stop();
  return client_.Pass();
}

void EnrollmentHandlerChromeOS::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);
  CHECK_EQ(STEP_POLICY_FETCH, enrollment_step_);

  enrollment_step_ = STEP_VALIDATION;

  // Validate the policy.
  const em::PolicyFetchResponse* policy = client_->GetPolicyFor(
      dm_protocol::kChromeDevicePolicyType, std::string());
  if (!policy) {
    ReportResult(EnrollmentStatus::ForFetchError(
        DM_STATUS_RESPONSE_DECODING_ERROR));
    return;
  }

  scoped_ptr<DeviceCloudPolicyValidator> validator(
      DeviceCloudPolicyValidator::Create(
          scoped_ptr<em::PolicyFetchResponse>(
              new em::PolicyFetchResponse(*policy)),
          background_task_runner_));

  validator->ValidateTimestamp(base::Time(), base::Time::NowFromSystemTime(),
                               CloudPolicyValidatorBase::TIMESTAMP_REQUIRED);

  // If this is re-enrollment, make sure that the new policy matches the
  // previously-enrolled domain.
  std::string domain;
  if (install_attributes_->IsEnterpriseDevice()) {
    domain = install_attributes_->GetDomain();
    validator->ValidateDomain(domain);
  }
  validator->ValidateDMToken(client->dm_token(),
                             CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  validator->ValidatePolicyType(dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  if (management_mode_ == MANAGEMENT_MODE_CONSUMER_MANAGED) {
    // For consumer-managed devices, although we don't store the policy, we
    // still need to verify its integrity since we use the request token in it.
    // The consumer device management server does not have the verification
    // key, and we need to skip checking on that by passing an empty key to
    // ValidateInitialKey(). ValidateInitialKey() still checks that the policy
    // data is correctly signed by the new public key when the verification key
    // is empty.
    validator->ValidateInitialKey(std::string(), std::string());
  } else {
    // If |domain| is empty here, the policy validation code will just use the
    // domain from the username field in the policy itself to do key validation.
    // TODO(mnissler): Plumb the enrolling user's username into this object so
    // we can validate the username on the resulting policy, and use the domain
    // from that username to validate the key below (http://crbug.com/343074).
    validator->ValidateInitialKey(GetPolicyVerificationKey(), domain);
  }
  validator.release()->StartValidation(
      base::Bind(&EnrollmentHandlerChromeOS::HandlePolicyValidationResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ == STEP_REGISTRATION && client_->is_registered()) {
    enrollment_step_ = STEP_POLICY_FETCH,
    device_mode_ = client_->device_mode();
    if (device_mode_ == DEVICE_MODE_NOT_SET)
      device_mode_ = DEVICE_MODE_ENTERPRISE;
    if (!allowed_device_modes_.test(device_mode_)) {
      LOG(ERROR) << "Bad device mode " << device_mode_;
      ReportResult(EnrollmentStatus::ForStatus(
          EnrollmentStatus::STATUS_REGISTRATION_BAD_MODE));
      return;
    }
    client_->FetchPolicy();
  } else {
    LOG(FATAL) << "Registration state changed to " << client_->is_registered()
               << " in step " << enrollment_step_ << ".";
  }
}

void EnrollmentHandlerChromeOS::OnClientError(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ == STEP_ROBOT_AUTH_FETCH) {
    LOG(ERROR) << "API authentication code fetch failed: "
               << client_->status();
    ReportResult(EnrollmentStatus::ForRobotAuthFetchError(client_->status()));
  } else if (enrollment_step_ < STEP_POLICY_FETCH) {
    ReportResult(EnrollmentStatus::ForRegistrationError(client_->status()));
  } else {
    ReportResult(EnrollmentStatus::ForFetchError(client_->status()));
  }
}

void EnrollmentHandlerChromeOS::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);

  if (enrollment_step_ == STEP_LOADING_STORE) {
    // If the |store_| wasn't initialized when StartEnrollment() was called,
    // then StartRegistration() bails silently. This gets registration rolling
    // again after the store finishes loading.
    StartRegistration();
  } else if (enrollment_step_ == STEP_STORE_POLICY) {
    ReportResult(EnrollmentStatus::ForStatus(EnrollmentStatus::STATUS_SUCCESS));
  }
}

void EnrollmentHandlerChromeOS::OnStoreError(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);
  if (enrollment_step_ == STEP_STORE_TOKEN_AND_ID) {
    // Calling OwnerSettingsServiceChromeOS::SetManagementSettings()
    // on a non- enterprise-managed device will fail as
    // DeviceCloudPolicyStore listens to all changes on device
    // settings, and it calls OnStoreError() when the device is not
    // enterprise-managed.
    return;
  }
  ReportResult(EnrollmentStatus::ForStoreError(store_->status(),
                                               store_->validation_status()));
}

void EnrollmentHandlerChromeOS::HandleStateKeysResult(
    const std::vector<std::string>& state_keys) {
  CHECK_EQ(STEP_STATE_KEYS, enrollment_step_);

  // Make sure state keys are available if forced re-enrollment is on.
  if (chromeos::AutoEnrollmentController::GetMode() ==
      chromeos::AutoEnrollmentController::MODE_FORCED_RE_ENROLLMENT) {
    client_->SetStateKeysToUpload(state_keys);
    current_state_key_ = state_keys_broker_->current_state_key();
    if (state_keys.empty() || current_state_key_.empty()) {
      ReportResult(
          EnrollmentStatus::ForStatus(EnrollmentStatus::STATUS_NO_STATE_KEYS));
      return;
    }
  }

  enrollment_step_ = STEP_LOADING_STORE;
  StartRegistration();
}

void EnrollmentHandlerChromeOS::StartRegistration() {
  CHECK_EQ(STEP_LOADING_STORE, enrollment_step_);
  if (store_->is_initialized()) {
    enrollment_step_ = STEP_REGISTRATION;
    client_->Register(
        em::DeviceRegisterRequest::DEVICE,
        EnrollmentModeToRegistrationFlavor(enrollment_config_.mode),
        auth_token_, client_id_, requisition_, current_state_key_);
  } else {
    // Do nothing. StartRegistration() will be called again from OnStoreLoaded()
    // after the CloudPolicyStore has initialized.
  }
}

void EnrollmentHandlerChromeOS::HandlePolicyValidationResult(
    DeviceCloudPolicyValidator* validator) {
  CHECK_EQ(STEP_VALIDATION, enrollment_step_);
  if (validator->success()) {
    policy_ = validator->policy().Pass();
    username_ = validator->policy_data()->username();
    device_id_ = validator->policy_data()->device_id();
    request_token_ = validator->policy_data()->request_token();
    enrollment_step_ = STEP_ROBOT_AUTH_FETCH;
    client_->FetchRobotAuthCodes(auth_token_);
  } else {
    ReportResult(EnrollmentStatus::ForValidationError(validator->status()));
  }
}

void EnrollmentHandlerChromeOS::OnRobotAuthCodesFetched(
    CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);
  CHECK_EQ(STEP_ROBOT_AUTH_FETCH, enrollment_step_);

  if (client->robot_api_auth_code().empty()) {
    // If the server doesn't provide an auth code, skip the robot auth setup.
    // This allows clients running against the test server to transparently skip
    // robot auth.
    skip_robot_auth_ = true;
    enrollment_step_ = STEP_LOCK_DEVICE;
    StartLockDevice();
    return;
  }

  enrollment_step_ = STEP_ROBOT_AUTH_REFRESH;

  gaia::OAuthClientInfo client_info;
  client_info.client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  client_info.client_secret =
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret();
  client_info.redirect_uri = "oob";

  // Use the system request context to avoid sending user cookies.
  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(
      g_browser_process->system_request_context()));
  gaia_oauth_client_->GetTokensFromAuthCode(client_info,
                                            client->robot_api_auth_code(),
                                            0 /* max_retries */,
                                            this);
}

// GaiaOAuthClient::Delegate callback for OAuth2 refresh token fetched.
void EnrollmentHandlerChromeOS::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  CHECK_EQ(STEP_ROBOT_AUTH_REFRESH, enrollment_step_);

  robot_refresh_token_ = refresh_token;

  enrollment_step_ = STEP_LOCK_DEVICE;
  StartLockDevice();
}

// GaiaOAuthClient::Delegate
void EnrollmentHandlerChromeOS::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  // We never use the code that should trigger this callback.
  LOG(FATAL) << "Unexpected callback invoked.";
}

// GaiaOAuthClient::Delegate OAuth2 error when fetching refresh token request.
void EnrollmentHandlerChromeOS::OnOAuthError() {
  CHECK_EQ(STEP_ROBOT_AUTH_REFRESH, enrollment_step_);
  // OnOAuthError is only called if the request is bad (malformed) or the
  // response is bad (empty access token returned).
  LOG(ERROR) << "OAuth protocol error while fetching API refresh token.";
  ReportResult(
      EnrollmentStatus::ForRobotRefreshFetchError(net::HTTP_BAD_REQUEST));
}

// GaiaOAuthClient::Delegate network error when fetching refresh token.
void EnrollmentHandlerChromeOS::OnNetworkError(int response_code) {
  CHECK_EQ(STEP_ROBOT_AUTH_REFRESH, enrollment_step_);
  LOG(ERROR) << "Network error while fetching API refresh token: "
             << response_code;
  ReportResult(
      EnrollmentStatus::ForRobotRefreshFetchError(response_code));
}

void EnrollmentHandlerChromeOS::StartLockDevice() {
  CHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  // Since this method is also called directly.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (management_mode_ == MANAGEMENT_MODE_CONSUMER_MANAGED) {
    CHECK(owner_settings_service_);

    // Consumer device enrollment doesn't use install attributes. Instead,
    // we put the information in the owners settings.
    enrollment_step_ = STEP_STORE_TOKEN_AND_ID;
    chromeos::OwnerSettingsServiceChromeOS::ManagementSettings settings;
    settings.management_mode = management_mode_;
    settings.request_token = request_token_;
    settings.device_id = device_id_;
    owner_settings_service_->SetManagementSettings(
        settings,
        base::Bind(&EnrollmentHandlerChromeOS::HandleSetManagementSettingsDone,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    install_attributes_->LockDevice(
        username_, device_mode_, device_id_,
        base::Bind(&EnrollmentHandlerChromeOS::HandleLockDeviceResult,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void EnrollmentHandlerChromeOS::HandleSetManagementSettingsDone(bool success) {
  CHECK_EQ(STEP_STORE_TOKEN_AND_ID, enrollment_step_);
  if (!success) {
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::STATUS_STORE_TOKEN_AND_ID_FAILED));
    return;
  }

  StartStoreRobotAuth();
}

void EnrollmentHandlerChromeOS::HandleLockDeviceResult(
    EnterpriseInstallAttributes::LockResult lock_result) {
  CHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  switch (lock_result) {
    case EnterpriseInstallAttributes::LOCK_SUCCESS:
      StartStoreRobotAuth();
      break;
    case EnterpriseInstallAttributes::LOCK_NOT_READY:
      // We wait up to |kLockRetryTimeoutMs| milliseconds and if it hasn't
      // succeeded by then show an error to the user and stop the enrollment.
      if (lockbox_init_duration_ < kLockRetryTimeoutMs) {
        // InstallAttributes not ready yet, retry later.
        LOG(WARNING) << "Install Attributes not ready yet will retry in "
                     << kLockRetryIntervalMs << "ms.";
        base::MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&EnrollmentHandlerChromeOS::StartLockDevice,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromMilliseconds(kLockRetryIntervalMs));
        lockbox_init_duration_ += kLockRetryIntervalMs;
      } else {
        HandleLockDeviceResult(EnterpriseInstallAttributes::LOCK_TIMEOUT);
      }
      break;
    case EnterpriseInstallAttributes::LOCK_TIMEOUT:
    case EnterpriseInstallAttributes::LOCK_BACKEND_INVALID:
    case EnterpriseInstallAttributes::LOCK_ALREADY_LOCKED:
    case EnterpriseInstallAttributes::LOCK_SET_ERROR:
    case EnterpriseInstallAttributes::LOCK_FINALIZE_ERROR:
    case EnterpriseInstallAttributes::LOCK_READBACK_ERROR:
    case EnterpriseInstallAttributes::LOCK_WRONG_DOMAIN:
    case EnterpriseInstallAttributes::LOCK_WRONG_MODE:
      ReportResult(EnrollmentStatus::ForLockError(lock_result));
      break;
  }
}

void EnrollmentHandlerChromeOS::StartStoreRobotAuth() {
  enrollment_step_ = STEP_STORE_ROBOT_AUTH;

  // Don't store the token if robot auth was skipped.
  if (skip_robot_auth_) {
    HandleStoreRobotAuthTokenResult(true);
    return;
  }

  chromeos::DeviceOAuth2TokenServiceFactory::Get()->SetAndSaveRefreshToken(
      robot_refresh_token_,
      base::Bind(&EnrollmentHandlerChromeOS::HandleStoreRobotAuthTokenResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::HandleStoreRobotAuthTokenResult(bool result) {
  CHECK_EQ(STEP_STORE_ROBOT_AUTH, enrollment_step_);

  if (!result) {
    LOG(ERROR) << "Failed to store API refresh token.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::STATUS_ROBOT_REFRESH_STORE_FAILED));
    return;
  }

  if (management_mode_ == MANAGEMENT_MODE_CONSUMER_MANAGED) {
    // For consumer management enrollment, we don't store the policy.
    ReportResult(EnrollmentStatus::ForStatus(EnrollmentStatus::STATUS_SUCCESS));
    return;
  }

  enrollment_step_ = STEP_STORE_POLICY;
  store_->InstallInitialPolicy(*policy_);
}

void EnrollmentHandlerChromeOS::Stop() {
  if (client_.get())
    client_->RemoveObserver(this);
  enrollment_step_ = STEP_FINISHED;
  weak_ptr_factory_.InvalidateWeakPtrs();
  completion_callback_.Reset();
}

void EnrollmentHandlerChromeOS::ReportResult(EnrollmentStatus status) {
  EnrollmentCallback callback = completion_callback_;
  Stop();

  if (status.status() != EnrollmentStatus::STATUS_SUCCESS) {
    LOG(WARNING) << "Enrollment failed: " << status.status()
                 << ", client: " << status.client_status()
                 << ", validation: " << status.validation_status()
                 << ", store: " << status.store_status()
                 << ", lock: " << status.lock_status();
  }

  if (!callback.is_null())
    callback.Run(status);
}

}  // namespace policy
