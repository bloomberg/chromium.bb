// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"

using content::MessageLoopRunner;
using content::NavigationController;
using content::WebContents;

class SupervisedUserResourceThrottleTest : public InProcessBrowserTest {
 protected:
  SupervisedUserResourceThrottleTest() : supervised_user_service_(NULL) {}
  virtual ~SupervisedUserResourceThrottleTest() {}

 private:
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  SupervisedUserService* supervised_user_service_;
};

void SupervisedUserResourceThrottleTest::SetUpOnMainThread() {
  supervised_user_service_ =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
}

void SupervisedUserResourceThrottleTest::SetUpCommandLine(
    CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
}

// Tests that showing the blocking interstitial for a WebContents without a
// SupervisedUserNavigationObserver doesn't crash.
IN_PROC_BROWSER_TEST_F(SupervisedUserResourceThrottleTest,
                       NoNavigationObserverBlock) {
  Profile* profile = browser()->profile();
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(profile);
  supervised_user_settings_service->SetLocalSettingForTesting(
      supervised_users::kContentPackDefaultFilteringBehavior,
      scoped_ptr<base::Value>(
          new base::FundamentalValue(SupervisedUserURLFilter::BLOCK)));

  scoped_ptr<WebContents> web_contents(
      WebContents::Create(WebContents::CreateParams(profile)));
  NavigationController& controller = web_contents->GetController();
  content::TestNavigationObserver observer(web_contents.get());
  controller.LoadURL(GURL("http://www.example.com"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  observer.Wait();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::PAGE_TYPE_INTERSTITIAL, entry->GetPageType());
}
