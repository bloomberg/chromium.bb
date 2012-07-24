// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/managed_value_store_cache.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/value_store/policy_value_store.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

ManagedValueStoreCache::ManagedValueStoreCache(
    policy::PolicyService* policy_service)
    : policy_service_(policy_service) {}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

scoped_refptr<base::MessageLoopProxy>
    ManagedValueStoreCache::GetMessageLoop() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

void ManagedValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const policy::PolicyMap& policies = policy_service_->GetPolicies(
      policy::POLICY_DOMAIN_EXTENSIONS, extension->id());
  PolicyValueStore store(&policies);
  callback.Run(&store);
}

void ManagedValueStoreCache::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // There's no real storage backing this cache, so this is a nop.
}

}  // namespace extensions
