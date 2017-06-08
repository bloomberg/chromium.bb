// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WITH_MANAGEMENT_POLICY_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WITH_MANAGEMENT_POLICY_APITEST_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_apitest.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"

// The ExtensionSettings policy affects host permissions which impacts several
// API integration tests. This class enables easy declaration of
// ExtensionSettings policies and functions commonly used during these tests.
class ExtensionApiTestWithManagementPolicy : public ExtensionApiTest {
 public:
  ExtensionApiTestWithManagementPolicy();
  ~ExtensionApiTestWithManagementPolicy() override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

 protected:
  policy::MockConfigurationPolicyProvider policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionApiTestWithManagementPolicy);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WITH_MANAGEMENT_POLICY_APITEST_H_
