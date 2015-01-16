// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_uma.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {

// A helper class that takes care of asynchronously revoking a given token.
class TokenRevoker : public GaiaAuthConsumer {
 public:
  TokenRevoker();
  ~TokenRevoker() override;

  void Start(const std::string& token);

  // GaiaAuthConsumer:
  void OnOAuth2RevokeTokenCompleted() override;

 private:
  GaiaAuthFetcher gaia_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TokenRevoker);
};

TokenRevoker::TokenRevoker()
    : gaia_fetcher_(this,
                    GaiaConstants::kChromeOSSource,
                    g_browser_process->system_request_context()) {
}

TokenRevoker::~TokenRevoker() {
}

void TokenRevoker::Start(const std::string& token) {
  gaia_fetcher_.StartRevokeOAuth2Token(token);
}

void TokenRevoker::OnOAuth2RevokeTokenCompleted() {
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace

namespace chromeos {

EnterpriseEnrollmentHelperImpl::EnterpriseEnrollmentHelperImpl(
    EnrollmentStatusConsumer* status_consumer,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain)
    : EnterpriseEnrollmentHelper(status_consumer),
      enrollment_config_(enrollment_config),
      enrolling_user_domain_(enrolling_user_domain),
      profile_(NULL),
      fetch_additional_token_(false),
      started_(false),
      oauth_fetchers_finished_(0),
      last_auth_error_(GoogleServiceAuthError::AuthErrorNone()),
      finished_(false),
      success_(false),
      auth_data_cleared_(false),
      browsing_data_remover_(NULL),
      weak_ptr_factory_(this) {
}

EnterpriseEnrollmentHelperImpl::~EnterpriseEnrollmentHelperImpl() {
  DCHECK(g_browser_process->IsShuttingDown() || !started_ ||
         (finished_ && (success_ || !profile_ || auth_data_cleared_)));
  if (browsing_data_remover_)
    browsing_data_remover_->RemoveObserver(this);
}

void EnterpriseEnrollmentHelperImpl::EnrollUsingProfile(
    Profile* profile,
    bool fetch_additional_token) {
  DCHECK(!started_);
  started_ = true;
  profile_ = profile;
  fetch_additional_token_ = fetch_additional_token;
  oauth_fetchers_.resize(fetch_additional_token_ ? 2 : 1);
  for (size_t i = 0; i < oauth_fetchers_.size(); ++i) {
    oauth_fetchers_[i] = new policy::PolicyOAuth2TokenFetcher(
        profile_->GetRequestContext(),
        g_browser_process->system_request_context(),
        base::Bind(&EnterpriseEnrollmentHelperImpl::OnTokenFetched,
                   weak_ptr_factory_.GetWeakPtr(),
                   i));
    oauth_fetchers_[i]->Start();
  }
}

void EnterpriseEnrollmentHelperImpl::EnrollUsingToken(
    const std::string& token) {
  DCHECK(!started_);
  started_ = true;
  DoEnrollUsingToken(token);
}

void EnterpriseEnrollmentHelperImpl::ClearAuth(const base::Closure& callback) {
  if (!profile_) {
    callback.Run();
    return;
  }
  auth_clear_callbacks_.push_back(callback);
  if (browsing_data_remover_)
    return;

  for (size_t i = 0; i < oauth_fetchers_.size(); ++i) {
    // Do not revoke the additional token if enrollment has finished
    // successfully.
    if (i == 1 && success_)
      continue;

    if (!oauth_fetchers_[i]->oauth2_access_token().empty())
      (new TokenRevoker())->Start(oauth_fetchers_[i]->oauth2_access_token());

    if (!oauth_fetchers_[i]->oauth2_refresh_token().empty())
      (new TokenRevoker())->Start(oauth_fetchers_[i]->oauth2_refresh_token());
  }
  oauth_fetchers_.clear();

  browsing_data_remover_ =
      BrowsingDataRemover::CreateForUnboundedRange(profile_);
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->Remove(BrowsingDataRemover::REMOVE_SITE_DATA,
                                 BrowsingDataHelper::UNPROTECTED_WEB);
}

void EnterpriseEnrollmentHelperImpl::DoEnrollUsingToken(
    const std::string& token) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged() &&
      connector->GetEnterpriseDomain() != enrolling_user_domain_) {
    LOG(ERROR) << "Trying to re-enroll to a different domain than "
               << connector->GetEnterpriseDomain();
    UMA(policy::kMetricEnrollmentPrecheckDomainMismatch);
    finished_ = true;
    status_consumer()->OnOtherError(OTHER_ERROR_DOMAIN_MISMATCH);
    return;
  }

  policy::DeviceCloudPolicyInitializer::AllowedDeviceModes device_modes;
  device_modes[policy::DEVICE_MODE_ENTERPRISE] = true;
  connector->ScheduleServiceInitialization(0);

  policy::DeviceCloudPolicyInitializer* dcp_initializer =
      connector->GetDeviceCloudPolicyInitializer();
  CHECK(dcp_initializer);
  dcp_initializer->StartEnrollment(
      policy::MANAGEMENT_MODE_ENTERPRISE_MANAGED,
      connector->device_management_service(),
      nullptr /* owner_settings_service */, enrollment_config_, token,
      device_modes,
      base::Bind(&EnterpriseEnrollmentHelperImpl::OnEnrollmentFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnterpriseEnrollmentHelperImpl::OnTokenFetched(
    size_t fetcher_index,
    const std::string& token,
    const GoogleServiceAuthError& error) {
  CHECK_LT(fetcher_index, oauth_fetchers_.size());

  if (error.state() != GoogleServiceAuthError::NONE)
    last_auth_error_ = error;

  ++oauth_fetchers_finished_;
  if (oauth_fetchers_finished_ != oauth_fetchers_.size())
    return;

  if (last_auth_error_.state() != GoogleServiceAuthError::NONE) {
    ReportAuthStatus(last_auth_error_);
    finished_ = true;
    status_consumer()->OnAuthError(last_auth_error_);
    return;
  }

  if (oauth_fetchers_.size() == 2)
    additional_token_ = oauth_fetchers_[1]->oauth2_access_token();
  DoEnrollUsingToken(oauth_fetchers_[0]->oauth2_access_token());
}

void EnterpriseEnrollmentHelperImpl::OnEnrollmentFinished(
    policy::EnrollmentStatus status) {
  ReportEnrollmentStatus(status);
  finished_ = true;
  if (status.status() == policy::EnrollmentStatus::STATUS_SUCCESS) {
    success_ = true;
    DCHECK(!fetch_additional_token_ || !additional_token_.empty());
    status_consumer()->OnDeviceEnrolled(additional_token_);
  } else {
    status_consumer()->OnEnrollmentError(status);
  }
}

void EnterpriseEnrollmentHelperImpl::ReportAuthStatus(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::WEB_LOGIN_REQUIRED:
      UMA(policy::kMetricEnrollmentLoginFailed);
      LOG(ERROR) << "Auth error " << error.state();
      break;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      UMA(policy::kMetricEnrollmentAccountNotSignedUp);
      LOG(ERROR) << "Account not signed up " << error.state();
      break;
    case GoogleServiceAuthError::ACCOUNT_DELETED:
      UMA(policy::kMetricEnrollmentAccountDeleted);
      LOG(ERROR) << "Account deleted " << error.state();
      break;
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      UMA(policy::kMetricEnrollmentAccountDisabled);
      LOG(ERROR) << "Account disabled " << error.state();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      UMA(policy::kMetricEnrollmentNetworkFailed);
      LOG(WARNING) << "Network error " << error.state();
      break;
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED();
      break;
  }
}

void EnterpriseEnrollmentHelperImpl::ReportEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.status()) {
    case policy::EnrollmentStatus::STATUS_SUCCESS:
      UMA(policy::kMetricEnrollmentOK);
      return;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_FAILED:
    case policy::EnrollmentStatus::STATUS_POLICY_FETCH_FAILED:
      switch (status.client_status()) {
        case policy::DM_STATUS_SUCCESS:
          NOTREACHED();
          break;
        case policy::DM_STATUS_REQUEST_INVALID:
          UMA(policy::kMetricEnrollmentRegisterPolicyPayloadInvalid);
          break;
        case policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
          UMA(policy::kMetricEnrollmentRegisterPolicyDeviceNotFound);
          break;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
          UMA(policy::kMetricEnrollmentRegisterPolicyDMTokenInvalid);
          break;
        case policy::DM_STATUS_SERVICE_ACTIVATION_PENDING:
          UMA(policy::kMetricEnrollmentRegisterPolicyActivationPending);
          break;
        case policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
          UMA(policy::kMetricEnrollmentRegisterPolicyDeviceIdConflict);
          break;
        case policy::DM_STATUS_SERVICE_POLICY_NOT_FOUND:
          UMA(policy::kMetricEnrollmentRegisterPolicyNotFound);
          break;
        case policy::DM_STATUS_REQUEST_FAILED:
          UMA(policy::kMetricEnrollmentRegisterPolicyRequestFailed);
          break;
        case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:
          UMA(policy::kMetricEnrollmentRegisterPolicyTempUnavailable);
          break;
        case policy::DM_STATUS_HTTP_STATUS_ERROR:
          UMA(policy::kMetricEnrollmentRegisterPolicyHttpError);
          break;
        case policy::DM_STATUS_RESPONSE_DECODING_ERROR:
          UMA(policy::kMetricEnrollmentRegisterPolicyResponseInvalid);
          break;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          UMA(policy::kMetricEnrollmentNotSupported);
          break;
        case policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
          UMA(policy::kMetricEnrollmentRegisterPolicyInvalidSerial);
          break;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          UMA(policy::kMetricEnrollmentRegisterPolicyMissingLicenses);
          break;
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          UMA(policy::kMetricEnrollmentRegisterPolicyDeprovisioned);
          break;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          UMA(policy::kMetricEnrollmentRegisterPolicyDomainMismatch);
          break;
      }
      break;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_BAD_MODE:
      UMA(policy::kMetricEnrollmentInvalidEnrollmentMode);
      break;
    case policy::EnrollmentStatus::STATUS_NO_STATE_KEYS:
      UMA(policy::kMetricEnrollmentNoStateKeys);
      break;
    case policy::EnrollmentStatus::STATUS_VALIDATION_FAILED:
      UMA(policy::kMetricEnrollmentPolicyValidationFailed);
      break;
    case policy::EnrollmentStatus::STATUS_STORE_ERROR:
      UMA(policy::kMetricEnrollmentCloudPolicyStoreError);
      break;
    case policy::EnrollmentStatus::STATUS_LOCK_ERROR:
      switch (status.lock_status()) {
        case policy::EnterpriseInstallAttributes::LOCK_SUCCESS:
        case policy::EnterpriseInstallAttributes::LOCK_NOT_READY:
          NOTREACHED();
          break;
        case policy::EnterpriseInstallAttributes::LOCK_TIMEOUT:
          UMA(policy::kMetricEnrollmentLockboxTimeoutError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_BACKEND_INVALID:
          UMA(policy::kMetricEnrollmentLockBackendInvalid);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_ALREADY_LOCKED:
          UMA(policy::kMetricEnrollmentLockAlreadyLocked);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_SET_ERROR:
          UMA(policy::kMetricEnrollmentLockSetError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_FINALIZE_ERROR:
          UMA(policy::kMetricEnrollmentLockFinalizeError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_READBACK_ERROR:
          UMA(policy::kMetricEnrollmentLockReadbackError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_WRONG_DOMAIN:
          UMA(policy::kMetricEnrollmentLockDomainMismatch);
          break;
      }
      break;
    case policy::EnrollmentStatus::STATUS_ROBOT_AUTH_FETCH_FAILED:
      UMA(policy::kMetricEnrollmentRobotAuthCodeFetchFailed);
      break;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_FETCH_FAILED:
      UMA(policy::kMetricEnrollmentRobotRefreshTokenFetchFailed);
      break;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_STORE_FAILED:
      UMA(policy::kMetricEnrollmentRobotRefreshTokenStoreFailed);
      break;
    case policy::EnrollmentStatus::STATUS_STORE_TOKEN_AND_ID_FAILED:
      // This error should not happen for enterprise enrollment, it only affects
      // consumer enrollment.
      UMA(policy::kMetricEnrollmentStoreTokenAndIdFailed);
      NOTREACHED();
      break;
  }
}

void EnterpriseEnrollmentHelperImpl::UMA(policy::MetricEnrollment sample) {
  EnrollmentUMA(sample, enrollment_config_.mode);
}

void EnterpriseEnrollmentHelperImpl::OnBrowsingDataRemoverDone() {
  browsing_data_remover_->RemoveObserver(this);
  browsing_data_remover_ = NULL;
  auth_data_cleared_ = true;

  std::vector<base::Closure> callbacks_to_run;
  callbacks_to_run.swap(auth_clear_callbacks_);
  for (std::vector<base::Closure>::iterator callback(callbacks_to_run.begin());
       callback != callbacks_to_run.end();
       ++callback) {
    callback->Run();
  }
}

}  // namespace chromeos
