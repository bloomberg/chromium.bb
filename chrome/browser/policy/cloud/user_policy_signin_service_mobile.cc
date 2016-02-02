// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

#if defined(OS_IOS)
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::IOS_BROWSER;
#elif defined(OS_ANDROID)
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::ANDROID_BROWSER;
#else
#error "This file can be built only on OS_IOS or OS_ANDROID."
#endif

}  // namespace

UserPolicySigninService::UserPolicySigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserCloudPolicyManager* policy_manager,
    SigninManager* signin_manager,
    scoped_refptr<net::URLRequestContextGetter> system_request_context,
    ProfileOAuth2TokenService* token_service)
    : UserPolicySigninServiceBase(profile,
                                  local_state,
                                  device_management_service,
                                  policy_manager,
                                  signin_manager,
                                  system_request_context),
      oauth2_token_service_(token_service),
      profile_prefs_(profile->GetPrefs()),
      weak_factory_(this) {
#if defined(OS_IOS)
  // iOS doesn't create this service with the Profile; instead it's created
  // a little bit later. See UserPolicySigninServiceFactory.
  InitializeOnProfileReady(profile);
#endif
}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::RegisterForPolicy(
    const std::string& username,
    const std::string& account_id,
    const PolicyRegistrationCallback& callback) {
  RegisterForPolicyInternal(username, account_id, "", callback);
}

#if !defined(OS_ANDROID)
void UserPolicySigninService::RegisterForPolicyWithAccessToken(
    const std::string& username,
    const std::string& access_token,
    const PolicyRegistrationCallback& callback) {
  RegisterForPolicyInternal(username, "", access_token, callback);
}

// static
std::vector<std::string> UserPolicySigninService::GetScopes() {
  return CloudPolicyClientRegistrationHelper::GetScopes();
}
#endif

void UserPolicySigninService::RegisterForPolicyInternal(
    const std::string& username,
    const std::string& account_id,
    const std::string& access_token,
    const PolicyRegistrationCallback& callback) {
  // Create a new CloudPolicyClient for fetching the DMToken.
  scoped_ptr<CloudPolicyClient> policy_client = CreateClientForRegistrationOnly(
      username);
  if (!policy_client) {
    callback.Run(std::string(), std::string());
    return;
  }

  CancelPendingRegistration();

  // Fire off the registration process. Callback keeps the CloudPolicyClient
  // alive for the length of the registration process.
  registration_helper_.reset(new CloudPolicyClientRegistrationHelper(
      policy_client.get(),
      kCloudPolicyRegistrationType));

  if (access_token.empty()) {
    registration_helper_->StartRegistration(
        oauth2_token_service_, account_id,
        base::Bind(&UserPolicySigninService::CallPolicyRegistrationCallback,
                   base::Unretained(this), base::Passed(&policy_client),
                   callback));
  } else {
#if defined(OS_ANDROID)
    NOTREACHED();
#else
    registration_helper_->StartRegistrationWithAccessToken(
        access_token,
        base::Bind(&UserPolicySigninService::CallPolicyRegistrationCallback,
                   base::Unretained(this),
                   base::Passed(&policy_client),
                   callback));
#endif
  }
}

void UserPolicySigninService::CallPolicyRegistrationCallback(
    scoped_ptr<CloudPolicyClient> client,
    PolicyRegistrationCallback callback) {
  registration_helper_.reset();
  callback.Run(client->dm_token(), client->client_id());
}

void UserPolicySigninService::Shutdown() {
  CancelPendingRegistration();
  registration_helper_.reset();
  UserPolicySigninServiceBase::Shutdown();
}

void UserPolicySigninService::OnInitializationCompleted(
    CloudPolicyService* service) {
  UserCloudPolicyManager* manager = policy_manager();
  DCHECK_EQ(service, manager->core()->service());
  DCHECK(service->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  if (manager->IsClientRegistered()) {
    DVLOG(1) << "Client already registered - not fetching DMToken";
    return;
  }

  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  base::TimeDelta retry_delay = base::TimeDelta::FromDays(3);
  if (connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
      connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    retry_delay = base::TimeDelta::FromDays(1);
  }

  base::Time last_check_time = base::Time::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kLastPolicyCheckTime));
  base::Time now = base::Time::Now();
  base::Time next_check_time = last_check_time + retry_delay;

  // Check immediately if no check was ever done before (last_check_time == 0),
  // or if the last check was in the future (?), or if we're already past the
  // next check time. Otherwise, delay checking until the next check time.
  base::TimeDelta try_registration_delay = base::TimeDelta::FromSeconds(5);
  if (now > last_check_time && now < next_check_time)
    try_registration_delay = next_check_time - now;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&UserPolicySigninService::RegisterCloudPolicyService,
                 weak_factory_.GetWeakPtr()),
      try_registration_delay);
}

void UserPolicySigninService::ShutdownUserCloudPolicyManager() {
  CancelPendingRegistration();
  UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager();
}

void UserPolicySigninService::RegisterCloudPolicyService() {
  // If the user signed-out while this task was waiting then Shutdown() would
  // have been called, which would have invalidated this task. Since we're here
  // then the user must still be signed-in.
  DCHECK(signin_manager()->IsAuthenticated());
  DCHECK(!policy_manager()->IsClientRegistered());
  DCHECK(policy_manager()->core()->client());

  // Persist the current time as the last policy registration attempt time.
  profile_prefs_->SetInt64(prefs::kLastPolicyCheckTime,
                           base::Time::Now().ToInternalValue());

  registration_helper_.reset(new CloudPolicyClientRegistrationHelper(
      policy_manager()->core()->client(),
      kCloudPolicyRegistrationType));
  registration_helper_->StartRegistration(
      oauth2_token_service_,
      signin_manager()->GetAuthenticatedAccountId(),
      base::Bind(&UserPolicySigninService::OnRegistrationDone,
                 base::Unretained(this)));
}

void UserPolicySigninService::CancelPendingRegistration() {
  weak_factory_.InvalidateWeakPtrs();
}

void UserPolicySigninService::OnRegistrationDone() {
  registration_helper_.reset();
}

}  // namespace policy
