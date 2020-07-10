// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GUEST_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_PROFILES_GUEST_MODE_POLICY_HANDLER_H_

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

class GuestModePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  GuestModePolicyHandler();
  ~GuestModePolicyHandler() override;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestModePolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_PROFILES_GUEST_MODE_POLICY_HANDLER_H_
