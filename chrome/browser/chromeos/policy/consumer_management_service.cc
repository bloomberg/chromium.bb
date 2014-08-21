// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "policy/proto/device_management_backend.pb.h"

namespace {

const char* kAttributeOwnerId = "consumer_management.owner_id";

}  // namespace

namespace policy {

ConsumerManagementService::ConsumerManagementService(
    chromeos::CryptohomeClient* client)
    : Consumer("consumer_management_service"),
      client_(client),
      enrolling_token_service_(NULL),
      weak_ptr_factory_(this) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
}

ConsumerManagementService::~ConsumerManagementService() {
  registrar_.Remove(this,
                    chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                    content::NotificationService::AllSources());
  if (enrolling_token_service_)
    enrolling_token_service_->RemoveObserver(this);
}

// static
void ConsumerManagementService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kConsumerManagementEnrollmentState, ENROLLMENT_NONE);
}

ConsumerManagementService::ConsumerEnrollmentState
ConsumerManagementService::GetEnrollmentState() const {
  const PrefService* prefs = g_browser_process->local_state();
  int state = prefs->GetInteger(prefs::kConsumerManagementEnrollmentState);
  if (state < 0 || state >= ENROLLMENT_LAST) {
    LOG(ERROR) << "Unknown enrollment state: " << state;
    state = 0;
  }
  return static_cast<ConsumerEnrollmentState>(state);
}

void ConsumerManagementService::SetEnrollmentState(
    ConsumerEnrollmentState state) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kConsumerManagementEnrollmentState, state);
}

void ConsumerManagementService::GetOwner(const GetOwnerCallback& callback) {
  cryptohome::GetBootAttributeRequest request;
  request.set_name(kAttributeOwnerId);
  client_->GetBootAttribute(
      request,
      base::Bind(&ConsumerManagementService::OnGetBootAttributeDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ConsumerManagementService::OnGetBootAttributeDone(
    const GetOwnerCallback& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool dbus_success,
    const cryptohome::BaseReply& reply) {
  if (!dbus_success || reply.error() != 0) {
    LOG(ERROR) << "Failed to get the owner info from boot lockbox.";
    callback.Run("");
    return;
  }

  callback.Run(
      reply.GetExtension(cryptohome::GetBootAttributeReply::reply).value());
}

void ConsumerManagementService::SetOwner(const std::string& user_id,
                                         const SetOwnerCallback& callback) {
  cryptohome::SetBootAttributeRequest request;
  request.set_name(kAttributeOwnerId);
  request.set_value(user_id.data(), user_id.size());
  client_->SetBootAttribute(
      request,
      base::Bind(&ConsumerManagementService::OnSetBootAttributeDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ConsumerManagementService::OnSetBootAttributeDone(
    const SetOwnerCallback& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool dbus_success,
    const cryptohome::BaseReply& reply) {
  if (!dbus_success || reply.error() != 0) {
    LOG(ERROR) << "Failed to set owner info in boot lockbox.";
    callback.Run(false);
    return;
  }

  cryptohome::FlushAndSignBootAttributesRequest request;
  client_->FlushAndSignBootAttributes(
      request,
      base::Bind(&ConsumerManagementService::OnFlushAndSignBootAttributesDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ConsumerManagementService::OnFlushAndSignBootAttributesDone(
    const SetOwnerCallback& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool dbus_success,
    const cryptohome::BaseReply& reply) {
  if (!dbus_success || reply.error() != 0) {
    LOG(ERROR) << "Failed to flush and sign boot lockbox.";
    callback.Run(false);
    return;
  }

  callback.Run(true);
}

void ConsumerManagementService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED) {
    NOTREACHED() << "Unexpected notification " << type;
    return;
  }

  Profile* profile = content::Details<Profile>(details).ptr();
  if (chromeos::ProfileHelper::IsOwnerProfile(profile))
    OnOwnerSignin(profile);
}

void ConsumerManagementService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (account_id == enrolling_account_id_) {
    enrolling_token_service_->RemoveObserver(this);
    OnOwnerRefreshTokenAvailable();
  }
}

void ConsumerManagementService::OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) {
  DCHECK_EQ(token_request_, request);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, token_request_.release());

  OnOwnerAccessTokenAvailable(access_token);
}

void ConsumerManagementService::OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) {
  DCHECK_EQ(token_request_, request);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, token_request_.release());

  LOG(ERROR) << "Failed to get the access token: " << error.ToString();
  EndEnrollment(ENROLLMENT_GET_TOKEN_FAILED);
}

void ConsumerManagementService::OnOwnerSignin(Profile* profile) {
  const ConsumerEnrollmentState state = GetEnrollmentState();
  switch (state) {
    case ENROLLMENT_NONE:
      // Do nothing.
      return;

    case ENROLLMENT_OWNER_STORED:
      // Continue the enrollment process after the owner signs in.
      ContinueEnrollmentProcess(profile);
      return;

    case ENROLLMENT_SUCCESS:
    case ENROLLMENT_CANCELED:
    case ENROLLMENT_BOOT_LOCKBOX_FAILED:
    case ENROLLMENT_DM_SERVER_FAILED:
    case ENROLLMENT_GET_TOKEN_FAILED:
      ShowDesktopNotificationAndResetState(state);
      return;

    case ENROLLMENT_REQUESTED:
    case ENROLLMENT_LAST:
      NOTREACHED() << "Unexpected enrollment state " << state;
      return;
  }
}

void ConsumerManagementService::ContinueEnrollmentProcess(Profile* profile) {
  // First, we need to ensure that the refresh token is available.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  enrolling_account_id_ = signin_manager->GetAuthenticatedAccountId();

  enrolling_token_service_ =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  if (enrolling_token_service_->RefreshTokenIsAvailable(
          enrolling_account_id_)) {
    OnOwnerRefreshTokenAvailable();
  } else {
    enrolling_token_service_->AddObserver(this);
  }
}

void ConsumerManagementService::OnOwnerRefreshTokenAvailable() {
  CHECK(enrolling_token_service_);

  // Now we can request the OAuth access token for device management to send the
  // device registration request to the device management server.
  OAuth2TokenService::ScopeSet oauth_scopes;
  oauth_scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);

  token_request_ = enrolling_token_service_->StartRequest(
      enrolling_account_id_, oauth_scopes, this);
}

void ConsumerManagementService::OnOwnerAccessTokenAvailable(
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

  initializer->StartEnrollment(
      enterprise_management::PolicyData::ENTERPRISE_MANAGED,
      connector->GetDeviceManagementServiceForConsumer(),
      access_token,
      false,  // is_auto_enrollment
      device_modes,
      base::Bind(&ConsumerManagementService::OnEnrollmentCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ConsumerManagementService::OnEnrollmentCompleted(EnrollmentStatus status) {
  if (status.status() != EnrollmentStatus::STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to enroll the device."
               << " status=" << status.status()
               << " client_status=" << status.client_status()
               << " http_status=" << status.http_status()
               << " store_status=" << status.store_status()
               << " validation_status=" << status.validation_status();
    EndEnrollment(ENROLLMENT_DM_SERVER_FAILED);
    return;
  }

  EndEnrollment(ENROLLMENT_SUCCESS);
}

void ConsumerManagementService::EndEnrollment(ConsumerEnrollmentState state) {
  SetEnrollmentState(state);
  if (user_manager::UserManager::Get()->IsCurrentUserOwner())
    ShowDesktopNotificationAndResetState(state);
}

void ConsumerManagementService::ShowDesktopNotificationAndResetState(
    ConsumerEnrollmentState state) {
  // TODO(davidyu): Show a desktop notification to the current user, who should
  // be the owner.
  SetEnrollmentState(ENROLLMENT_NONE);
}

}  // namespace policy
