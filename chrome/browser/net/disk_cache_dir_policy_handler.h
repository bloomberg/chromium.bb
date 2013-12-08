// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DISK_CACHE_DIR_POLICY_HANDLER_H_
#define CHROME_BROWSER_NET_DISK_CACHE_DIR_POLICY_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the DiskCacheDir policy.
class DiskCacheDirPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  DiskCacheDirPolicyHandler();
  virtual ~DiskCacheDirPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiskCacheDirPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_NET_DISK_CACHE_DIR_POLICY_HANDLER_H_
