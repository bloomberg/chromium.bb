// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"

ExtensionApiTestWithManagementPolicy::ExtensionApiTestWithManagementPolicy()
    : ExtensionApiTest() {}

ExtensionApiTestWithManagementPolicy::~ExtensionApiTestWithManagementPolicy() {}

void ExtensionApiTestWithManagementPolicy::SetUpInProcessBrowserTestFixture() {
  ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  policy_provider_.SetAutoRefresh();
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);
}

void ExtensionApiTestWithManagementPolicy::SetUpOnMainThread() {
  ExtensionApiTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
}
