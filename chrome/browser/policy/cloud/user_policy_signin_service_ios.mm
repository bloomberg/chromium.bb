// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_ios.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

enterprise_management::DeviceRegisterRequest::Type GetRegistrationType() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kFakeCloudPolicyType))
    return enterprise_management::DeviceRegisterRequest::BROWSER;
  return enterprise_management::DeviceRegisterRequest::IOS_BROWSER;
}

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
      weak_factory_(this),
      oauth2_token_service_(token_service),
      profile_prefs_(profile->GetPrefs()) {}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::RegisterForPolicy(
    const std::string& username,
    PolicyRegistrationBlockCallback callback) {
  // Create a new CloudPolicyClient for fetching the DMToken.
  scoped_ptr<CloudPolicyClient> policy_client = CreateClientForRegistrationOnly(
      username);
  if (!policy_client) {
    callback(std::string(), std::string());
    return;
  }

  CancelPendingRegistration();

  // Fire off the registration process. Callback keeps the CloudPolicyClient
  // alive for the length of the registration process.
  registration_helper_.reset(new CloudPolicyClientRegistrationHelper(
      policy_client.get(),
      GetRegistrationType()));
  registration_helper_->StartRegistration(
      oauth2_token_service_,
      username,
      base::Bind(&UserPolicySigninService::CallPolicyRegistrationCallback,
                 base::Unretained(this),
                 base::Passed(&policy_client),
                 [callback copy]));
}

void UserPolicySigninService::FetchPolicy(
    const std::string& username,
    const std::string& dm_token,
    const std::string& client_id,
    scoped_refptr<net::URLRequestContextGetter> profile_request_context,
    PolicyFetchBlockCallback callback) {
  FetchPolicyForSignedInUser(
      username,
      dm_token,
      client_id,
      profile_request_context,
      base::Bind(&UserPolicySigninService::CallPolicyFetchCallback,
                 base::Unretained(this),
                 [callback copy]));
}

void UserPolicySigninService::CallPolicyRegistrationCallback(
    scoped_ptr<CloudPolicyClient> client,
    PolicyRegistrationBlockCallback callback) {
  registration_helper_.reset();
  callback(client->dm_token(), client->client_id());
  [callback release];
}

void UserPolicySigninService::CallPolicyFetchCallback(
    PolicyFetchBlockCallback callback,
    bool succeeded) {
  callback(succeeded);
  [callback release];
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
  // Note: we don't register the cloud policy client on iOS if it's not
  // registered at this stage. If there was no policy at sign-in time then
  // there won't be policy later either.
}

void UserPolicySigninService::CancelPendingRegistration() {
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace policy
