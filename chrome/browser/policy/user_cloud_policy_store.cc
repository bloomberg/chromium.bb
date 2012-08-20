// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_store.h"

#include "base/bind.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"

using enterprise_management::PolicyData;

namespace policy {

UserCloudPolicyStore::UserCloudPolicyStore(Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      profile_(profile) {
}

UserCloudPolicyStore::~UserCloudPolicyStore() {
}

void UserCloudPolicyStore::Load() {
  // TODO(atwilson): Read policy from file.
  policy_.reset();
  policy_map_.Clear();
  NotifyStoreLoaded();
}

void UserCloudPolicyStore::RemoveStoredPolicy() {
  // TODO(atwilson): Remove policy from disk.
}

void UserCloudPolicyStore::Store(
    const enterprise_management::PolicyFetchResponse& policy) {
  // Stop any pending requests to store policy, then validate the new policy
  // before storing it.
  weak_factory_.InvalidateWeakPtrs();
  scoped_ptr<enterprise_management::PolicyFetchResponse> policy_copy(
      new enterprise_management::PolicyFetchResponse(policy));
  Validate(policy_copy.Pass(),
           base::Bind(&UserCloudPolicyStore::StorePolicyAfterValidation,
                      weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStore::Validate(
    scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  // Configure the validator.
  scoped_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy.Pass(), callback);
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  std::string username = signin->GetAuthenticatedUsername();
  DCHECK(!username.empty());
  validator->ValidateUsername(username);

  // Start validation. The Validator will free itself once validation is
  // complete.
  validator.release()->StartValidation();
}

void UserCloudPolicyStore::StorePolicyAfterValidation(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }
  // TODO(atwilson): Write policy to file.
  InstallPolicy(validator->policy_data().Pass(), validator->payload().Pass());
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

// static
scoped_ptr<CloudPolicyStore> CloudPolicyStore::CreateUserPolicyStore(
    Profile* profile) {
  return scoped_ptr<CloudPolicyStore>(new UserCloudPolicyStore(profile));
}

}  // namespace policy
