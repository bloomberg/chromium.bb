// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_POLICY_HANDLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_POLICY_HANDLER_H_

#include "chrome/browser/policy/configuration_policy_handler.h"

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
  virtual void ApplyPolicySettings(const policy::PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadDirPolicyHandler);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_POLICY_HANDLER_H_
