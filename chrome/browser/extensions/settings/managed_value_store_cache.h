// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_MANAGED_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_MANAGED_VALUE_STORE_CACHE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/settings/settings_observer.h"
#include "chrome/browser/extensions/settings/value_store_cache.h"
#include "chrome/browser/policy/policy_service.h"

namespace extensions {

// Runs the StorageCallback with a read-only ValueStore that pulls values from
// the PolicyService for the given extension.
class ManagedValueStoreCache : public ValueStoreCache,
                               public policy::PolicyService::Observer {
 public:
  ManagedValueStoreCache(policy::PolicyService* policy_service,
                         scoped_refptr<SettingsObserverList> observers);
  virtual ~ManagedValueStoreCache();

  // ValueStoreCache implementation:

  virtual void ShutdownOnUI() OVERRIDE;

  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() const OVERRIDE;

  virtual void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) OVERRIDE;

  virtual void DeleteStorageSoon(const std::string& extension_id) OVERRIDE;

  // PolicyService::Observer implementation:

  virtual void OnPolicyUpdated(policy::PolicyDomain domain,
                               const std::string& component_id,
                               const policy::PolicyMap& previous,
                               const policy::PolicyMap& current) OVERRIDE;

 private:
  policy::PolicyService* policy_service_;

  scoped_refptr<SettingsObserverList> observers_;

  DISALLOW_COPY_AND_ASSIGN(ManagedValueStoreCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_MANAGED_VALUE_STORE_CACHE_H_
