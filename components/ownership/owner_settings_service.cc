// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_settings_service.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "components/ownership/owner_key_util.h"
#include "crypto/signature_creator.h"

namespace em = enterprise_management;

namespace ownership {

namespace {

std::string AssembleAndSignPolicy(scoped_ptr<em::PolicyData> policy,
                                  crypto::RSAPrivateKey* private_key) {
  // Assemble the policy.
  em::PolicyFetchResponse policy_response;
  if (!policy->SerializeToString(policy_response.mutable_policy_data())) {
    LOG(ERROR) << "Failed to encode policy payload.";
    return std::string();
  }

  // Generate the signature.
  scoped_ptr<crypto::SignatureCreator> signature_creator(
      crypto::SignatureCreator::Create(private_key,
                                       crypto::SignatureCreator::SHA1));
  signature_creator->Update(
      reinterpret_cast<const uint8*>(policy_response.policy_data().c_str()),
      policy_response.policy_data().size());
  std::vector<uint8> signature_bytes;
  std::string policy_blob;
  if (!signature_creator->Final(&signature_bytes)) {
    LOG(ERROR) << "Failed to create policy signature.";
    return std::string();
  }

  policy_response.mutable_policy_data_signature()->assign(
      reinterpret_cast<const char*>(vector_as_array(&signature_bytes)),
      signature_bytes.size());
  return policy_response.SerializeAsString();
}

}  // namepace

OwnerSettingsService::OwnerSettingsService(
    const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util)
    : owner_key_util_(owner_key_util), weak_factory_(this) {
}

OwnerSettingsService::~OwnerSettingsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool OwnerSettingsService::IsOwner() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return private_key_.get() && private_key_->key();
}

void OwnerSettingsService::IsOwnerAsync(const IsOwnerCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (private_key_.get()) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, IsOwner()));
  } else {
    pending_is_owner_callbacks_.push_back(callback);
  }
}

bool OwnerSettingsService::AssembleAndSignPolicyAsync(
    base::TaskRunner* task_runner,
    scoped_ptr<em::PolicyData> policy,
    const AssembleAndSignPolicyAsyncCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!task_runner || !IsOwner())
    return false;
  return base::PostTaskAndReplyWithResult(
      task_runner,
      FROM_HERE,
      base::Bind(
          &AssembleAndSignPolicy, base::Passed(&policy), private_key_->key()),
      callback);
}

void OwnerSettingsService::ReloadKeypair() {
  ReloadKeypairImpl(
      base::Bind(&OwnerSettingsService::OnKeypairLoaded, as_weak_ptr()));
}

void OwnerSettingsService::OnKeypairLoaded(
    const scoped_refptr<PublicKey>& public_key,
    const scoped_refptr<PrivateKey>& private_key) {
  DCHECK(thread_checker_.CalledOnValidThread());

  public_key_ = public_key;
  private_key_ = private_key;

  const bool is_owner = IsOwner();
  std::vector<IsOwnerCallback> is_owner_callbacks;
  is_owner_callbacks.swap(pending_is_owner_callbacks_);
  for (std::vector<IsOwnerCallback>::iterator it(is_owner_callbacks.begin());
       it != is_owner_callbacks.end();
       ++it) {
    it->Run(is_owner);
  }

  OnPostKeypairLoadedActions();
}

}  // namespace ownership
