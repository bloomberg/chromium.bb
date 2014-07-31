// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

class EasyUnlockServiceTest : public InProcessBrowserTest {
 public:
  EasyUnlockServiceTest() {}
  virtual ~EasyUnlockServiceTest() {}

  void SetEasyUnlockAllowedPolicy(bool allowed) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kEasyUnlockAllowed,
               policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER,
               new base::FundamentalValue(allowed),
               NULL);
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

#if defined(GOOGLE_CHROME_BUILD)
  bool HasEasyUnlockApp() const {
    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(profile());
    ExtensionService* extension_service = extension_system->extension_service();
    return !!extension_service->GetExtensionById(
        extension_misc::kEasyUnlockAppId, false);
  }
#endif

  // InProcessBrowserTest:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  Profile* profile() const { return browser()->profile(); }

  EasyUnlockService* service() const {
    return EasyUnlockService::Get(profile());
  }

 private:
  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceTest);
};

// Tests that EasyUnlock is on by default.
IN_PROC_BROWSER_TEST_F(EasyUnlockServiceTest, DefaultOn) {
  EXPECT_TRUE(service()->IsAllowed());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(HasEasyUnlockApp());
#endif
}

class EasyUnlockServiceFinchEnabledTest : public EasyUnlockServiceTest {
 public:
  EasyUnlockServiceFinchEnabledTest() {}
  virtual ~EasyUnlockServiceFinchEnabledTest() {}

  // InProcessBrowserTest:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "EasyUnlock/Enable/");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceFinchEnabledTest);
};

// Tests that policy can override finch to turn easy unlock off.
IN_PROC_BROWSER_TEST_F(EasyUnlockServiceFinchEnabledTest, PolicyOveride) {
  EXPECT_TRUE(service()->IsAllowed());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(HasEasyUnlockApp());
#endif

  // Overridden by policy.
  SetEasyUnlockAllowedPolicy(false);
  EXPECT_FALSE(service()->IsAllowed());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_FALSE(HasEasyUnlockApp());
#endif

  SetEasyUnlockAllowedPolicy(true);
  EXPECT_TRUE(service()->IsAllowed());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(HasEasyUnlockApp());
#endif
}

class EasyUnlockServiceFinchDisabledTest : public EasyUnlockServiceTest {
 public:
  EasyUnlockServiceFinchDisabledTest() {}
  virtual ~EasyUnlockServiceFinchDisabledTest() {}

  // InProcessBrowserTest:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "EasyUnlock/Disable/");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceFinchDisabledTest);
};

// Tests that easy unlock is off when finch is disabled.
IN_PROC_BROWSER_TEST_F(EasyUnlockServiceFinchDisabledTest, StayDisabled) {
  // Finch is disabled.
  EXPECT_FALSE(service()->IsAllowed());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_FALSE(HasEasyUnlockApp());
#endif

  // Easy unlock stays off even when it is allowed by policy.
  SetEasyUnlockAllowedPolicy(true);
  EXPECT_FALSE(service()->IsAllowed());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_FALSE(HasEasyUnlockApp());
#endif
}
