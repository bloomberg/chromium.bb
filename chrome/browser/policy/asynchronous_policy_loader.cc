// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_loader.h"

#include "base/message_loop.h"
#include "base/task.h"

namespace policy {

AsynchronousPolicyLoader::AsynchronousPolicyLoader(
    AsynchronousPolicyProvider::Delegate* delegate)
    : delegate_(delegate),
      provider_(NULL),
      origin_loop_(MessageLoop::current()) {}

void AsynchronousPolicyLoader::Init() {
  policy_.reset(delegate_->Load());
}

void AsynchronousPolicyLoader::Stop() {
  delegate_.reset();
}

void AsynchronousPolicyLoader::SetProvider(
    AsynchronousPolicyProvider* provider) {
  provider_ = provider;
}

AsynchronousPolicyLoader::~AsynchronousPolicyLoader() {
}

// Manages the life cycle of a new policy map during until it's life cycle is
// taken over by the policy loader.
class UpdatePolicyTask : public Task {
 public:
  UpdatePolicyTask(scoped_refptr<AsynchronousPolicyLoader> loader,
                   DictionaryValue* new_policy)
      : loader_(loader),
        new_policy_(new_policy) {}

  virtual void Run() {
    loader_->UpdatePolicy(new_policy_.release());
  }

 private:
  scoped_refptr<AsynchronousPolicyLoader> loader_;
  scoped_ptr<DictionaryValue> new_policy_;
  DISALLOW_COPY_AND_ASSIGN(UpdatePolicyTask);
};

void AsynchronousPolicyLoader::Reload() {
  if (delegate_.get()) {
    DictionaryValue* new_policy = delegate_->Load();
    PostUpdatePolicyTask(new_policy);
  }
}

void AsynchronousPolicyLoader::PostUpdatePolicyTask(
    DictionaryValue* new_policy) {
  origin_loop_->PostTask(FROM_HERE, new UpdatePolicyTask(this, new_policy));
}

void AsynchronousPolicyLoader::UpdatePolicy(DictionaryValue* new_policy_raw) {
  scoped_ptr<DictionaryValue> new_policy(new_policy_raw);
  DCHECK(policy_.get());
  if (!policy_->Equals(new_policy.get())) {
    policy_.reset(new_policy.release());
    // TODO(danno): Change the notification between the provider and the
    // PrefStore into a notification mechanism, removing the need for the
    // WeakPtr for the provider.
    if (provider_)
      provider_->NotifyStoreOfPolicyChange();
  }
}

}  // namespace policy
