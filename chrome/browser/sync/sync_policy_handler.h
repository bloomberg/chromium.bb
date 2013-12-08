// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_POLICY_HANDLER_H_
#define CHROME_BROWSER_SYNC_SYNC_POLICY_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace browser_sync {

class PolicyMap;

// ConfigurationPolicyHandler for the SyncDisabled policy.
class SyncPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  SyncPolicyHandler();
  virtual ~SyncPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const policy::PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncPolicyHandler);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SYNC_POLICY_HANDLER_H_
