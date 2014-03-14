// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_POLICY_HANDLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_POLICY_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyMap;
}  // namespace policy

// ConfigurationPolicyHandler for the DownloadDirectory policy.
class DownloadDirPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  DownloadDirPolicyHandler();
  virtual ~DownloadDirPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const policy::PolicyMap& policies,
                                   policy::PolicyErrorMap* errors) OVERRIDE;

  virtual void ApplyPolicySettingsWithParameters(
      const policy::PolicyMap& policies,
      const policy::PolicyHandlerParameters& parameters,
      PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadDirPolicyHandler);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_POLICY_HANDLER_H_
