// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_android.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/policy/cloud/cloud_policy_client_registration_helper.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

namespace policy {

UserPolicySigninService::UserPolicySigninService(Profile* profile)
    : UserPolicySigninServiceBase(profile) {}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::RegisterPolicyClient(
    const std::string& username,
    const PolicyRegistrationCallback& callback) {
  // Create a new CloudPolicyClient for fetching the DMToken.
  scoped_ptr<CloudPolicyClient> policy_client = PrepareToRegister(username);
  if (!policy_client) {
    callback.Run(policy_client.Pass());
    return;
  }

  // Fire off the registration process. Callback keeps the CloudPolicyClient
  // alive for the length of the registration process.
  // TODO(joaodasilva): use DeviceRegisterRequest::ANDROID_BROWSER here once
  // the server is ready. http://crbug.com/248527
  const bool force_load_policy = false;
  registration_helper_.reset(new CloudPolicyClientRegistrationHelper(
      profile()->GetRequestContext(),
      policy_client.get(),
      force_load_policy,
      enterprise_management::DeviceRegisterRequest::BROWSER));
  registration_helper_->StartRegistration(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile()),
      username,
      base::Bind(&UserPolicySigninService::CallPolicyRegistrationCallback,
                 base::Unretained(this),
                 base::Passed(&policy_client),
                 callback));
}

void UserPolicySigninService::CallPolicyRegistrationCallback(
    scoped_ptr<CloudPolicyClient> client,
    PolicyRegistrationCallback callback) {
  registration_helper_.reset();
  if (!client->is_registered()) {
    // Registration failed, so free the client and pass NULL to the callback.
    client.reset();
  }
  callback.Run(client.Pass());
}

void UserPolicySigninService::Shutdown() {
  registration_helper_.reset();
  UserPolicySigninServiceBase::Shutdown();
}

void UserPolicySigninService::OnInitializationCompleted(
    CloudPolicyService* service) {
  UserCloudPolicyManager* manager = GetManager();
  DCHECK_EQ(service, manager->core()->service());
  DCHECK(service->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  if (manager->IsClientRegistered()) {
    DVLOG(1) << "Client already registered - not fetching DMToken";
  } else {
    // TODO(joaodasilva): figure out what to do in this case: a signed-in user
    // just started Chrome but hasn't registered for policy yet.
    // If a registration attempt is started from here then make sure this
    // doesn't happen on every startup.
  }
}

}  // namespace policy
