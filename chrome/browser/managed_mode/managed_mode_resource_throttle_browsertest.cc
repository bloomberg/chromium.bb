// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"

#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_utils.h"
#include "policy/policy_constants.h"

using content::MessageLoopRunner;
using content::NavigationController;
using content::WebContents;

class ManagedModeResourceThrottleTest : public InProcessBrowserTest {
 protected:
  ManagedModeResourceThrottleTest() : managed_user_service_(NULL) {}
  virtual ~ManagedModeResourceThrottleTest() {}

 private:
  virtual void SetUpOnMainThread() OVERRIDE;

  ManagedUserService* managed_user_service_;
};

void ManagedModeResourceThrottleTest::SetUpOnMainThread() {
  managed_user_service_ =
      ManagedUserServiceFactory::GetForProfile(browser()->profile());
  managed_user_service_->InitForTesting();
}

// Tests that showing the blocking interstitial for a WebContents without a
// ManagedModeNavigationObserver doesn't crash.
IN_PROC_BROWSER_TEST_F(ManagedModeResourceThrottleTest,
                       NoNavigationObserverBlock) {
  Profile* profile = browser()->profile();
  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForProfile(profile);
  policy::ManagedModePolicyProvider* policy_provider =
      connector->managed_mode_policy_provider();
  policy_provider->SetPolicy(
      policy::key::kContentPackDefaultFilteringBehavior,
      scoped_ptr<base::Value>(
          new base::FundamentalValue(ManagedModeURLFilter::BLOCK)));
  base::RunLoop().RunUntilIdle();

  scoped_ptr<WebContents> web_contents(
      WebContents::Create(WebContents::CreateParams(profile)));
  NavigationController& controller = web_contents->GetController();
  content::TestNavigationObserver observer(
      content::Source<NavigationController>(&controller), 1);
  controller.LoadURL(GURL("http://www.example.com"), content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  observer.Wait();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::PAGE_TYPE_INTERSTITIAL, entry->GetPageType());
}
