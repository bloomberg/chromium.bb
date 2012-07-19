// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_MANAGED_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_MANAGED_VALUE_STORE_CACHE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/settings/value_store_cache.h"

namespace policy {
class PolicyService;
}  // namespace policy

namespace extensions {

// TODO(joaodasilva): this is a work in progress. http://crbug.com/108992
class ManagedValueStoreCache : public ValueStoreCache {
 public:
  explicit ManagedValueStoreCache(policy::PolicyService* policy_service);
  virtual ~ManagedValueStoreCache();

  // ValueStoreCache implementation:

  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() const OVERRIDE;

  virtual void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) OVERRIDE;

  virtual void DeleteStorageSoon(const std::string& extension_id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedValueStoreCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_MANAGED_VALUE_STORE_CACHE_H_
