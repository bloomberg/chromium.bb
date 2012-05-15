// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_provider.h"

#include "base/bind.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace policy {

AsynchronousPolicyProvider::AsynchronousPolicyProvider(
    const PolicyDefinitionList* policy_list,
    scoped_refptr<AsynchronousPolicyLoader> loader)
    : ConfigurationPolicyProvider(policy_list),
      loader_(loader),
      pending_refreshes_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  loader_->Init(
      base::Bind(&AsynchronousPolicyProvider::OnLoaderReloaded,
                 base::Unretained(this)));
}

AsynchronousPolicyProvider::~AsynchronousPolicyProvider() {
  DCHECK(CalledOnValidThread());
  // |loader_| won't invoke its callback anymore after Stop(), therefore
  // Unretained(this) is safe in the ctor.
  loader_->Stop();
}

void AsynchronousPolicyProvider::RefreshPolicies() {
  DCHECK(CalledOnValidThread());
  pending_refreshes_++;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AsynchronousPolicyProvider::PostReloadOnFileThread,
                 loader_),
      base::Bind(&AsynchronousPolicyProvider::OnReloadPosted,
                 weak_ptr_factory_.GetWeakPtr()));
}

// static
void AsynchronousPolicyProvider::PostReloadOnFileThread(
    AsynchronousPolicyLoader* loader) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AsynchronousPolicyLoader::Reload, loader, true));
}

void AsynchronousPolicyProvider::OnReloadPosted() {
  DCHECK(CalledOnValidThread());
  pending_refreshes_--;
}

void AsynchronousPolicyProvider::OnLoaderReloaded(
    scoped_ptr<PolicyBundle> bundle) {
  DCHECK(CalledOnValidThread());
  if (pending_refreshes_ == 0)
    UpdatePolicy(bundle.Pass());
}

}  // namespace policy
