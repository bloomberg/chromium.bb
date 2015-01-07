// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_enrollment_handler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace policy {

ConsumerEnrollmentHandler::ConsumerEnrollmentHandler(
    Profile* profile,
    ConsumerManagementService* consumer_management_service,
    DeviceManagementService* device_management_service)
    : Consumer("consumer_enrollment_handler"),
      profile_(profile),
      consumer_management_service_(consumer_management_service),
      device_management_service_(device_management_service),
      weak_ptr_factory_(this) {
  gaia_account_id_ = SigninManagerFactory::GetForProfile(profile)->
      GetAuthenticatedAccountId();
  ContinueEnrollmentProcess();
}

ConsumerEnrollmentHandler::~ConsumerEnrollmentHandler() {
}

void ConsumerEnrollmentHandler::Shutdown() {
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
      RemoveObserver(this);
}

void ConsumerEnrollmentHandler::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (account_id == gaia_account_id_) {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
        RemoveObserver(this);
    OnOwnerRefreshTokenAvailable();
  }
}

void ConsumerEnrollmentHandler::OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) {
  DCHECK_EQ(token_request_, request);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, token_request_.release());

  OnOwnerAccessTokenAvailable(access_token);
}

void ConsumerEnrollmentHandler::OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) {
  DCHECK_EQ(token_request_, request);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, token_request_.release());

  LOG(ERROR) << "Failed to get the access token: " << error.ToString();
  EndEnrollment(ConsumerManagementStage::EnrollmentGetTokenFailed());
}

void ConsumerEnrollmentHandler::ContinueEnrollmentProcess() {
  // First, we need to ensure that the refresh token is available.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  if (token_service->RefreshTokenIsAvailable(gaia_account_id_)) {
    OnOwnerRefreshTokenAvailable();
  } else {
    token_service->AddObserver(this);
  }
}

void ConsumerEnrollmentHandler::OnOwnerRefreshTokenAvailable() {
  // Now we can request the OAuth access token for device management to send the
  // device registration request to the device management server.
  OAuth2TokenService::ScopeSet oauth_scopes;
  oauth_scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_request_ = ProfileOAuth2TokenServiceFactory::GetForProfile(
      profile_)->StartRequest(gaia_account_id_, oauth_scopes, this);
}

void ConsumerEnrollmentHandler::OnOwnerAccessTokenAvailable(
    const std::string& access_token) {
  // Now that we have the access token, we got everything we need to send the
  // device registration request to the device management server.
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceCloudPolicyInitializer* initializer =
      connector->GetDeviceCloudPolicyInitializer();
  CHECK(initializer);

  policy::DeviceCloudPolicyInitializer::AllowedDeviceModes device_modes;
  device_modes[policy::DEVICE_MODE_ENTERPRISE] = true;

  EnrollmentConfig enrollment_config;
  enrollment_config.mode = EnrollmentConfig::MODE_MANUAL;
  initializer->StartEnrollment(
      MANAGEMENT_MODE_CONSUMER_MANAGED, device_management_service_,
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_),
      enrollment_config, access_token, device_modes,
      base::Bind(&ConsumerEnrollmentHandler::OnEnrollmentCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ConsumerEnrollmentHandler::OnEnrollmentCompleted(EnrollmentStatus status) {
  if (status.status() != EnrollmentStatus::STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to enroll the device."
               << " status=" << status.status()
               << " client_status=" << status.client_status()
               << " http_status=" << status.http_status()
               << " store_status=" << status.store_status()
               << " validation_status=" << status.validation_status();
    EndEnrollment(ConsumerManagementStage::EnrollmentDMServerFailed());
    return;
  }

  EndEnrollment(ConsumerManagementStage::EnrollmentSuccess());
}

void ConsumerEnrollmentHandler::EndEnrollment(
    const ConsumerManagementStage& stage) {
  consumer_management_service_->SetStage(stage);
}

}  // namespace policy
