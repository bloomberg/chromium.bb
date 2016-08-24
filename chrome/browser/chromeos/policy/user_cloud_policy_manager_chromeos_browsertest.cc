// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Helper class that counts the number of notifications of the specified
// type that have been received.
class CountNotificationObserver : public content::NotificationObserver {
 public:
  CountNotificationObserver(int notification_type_to_count,
                            const content::NotificationSource& source)
      : notification_count_(0) {
    registrar_.Add(this, notification_type_to_count, source);
  }

  // NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    // Count the number of notifications seen.
    notification_count_++;
  }

  int notification_count() const { return notification_count_; }

 private:
  int notification_count_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CountNotificationObserver);
};

}  // namespace

namespace policy {

class UserCloudPolicyManagerTest : public LoginPolicyTestBase {
 protected:
  UserCloudPolicyManagerTest() {}

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    std::unique_ptr<base::ListValue> list(new base::ListValue);
    list->AppendString("chrome://policy");
    list->AppendString("chrome://about");

    policy->Set(key::kRestoreOnStartupURLs, std::move(list));
    policy->SetInteger(key::kRestoreOnStartup,
                       SessionStartupPref::kPrefValueURLs);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerTest);
};

IN_PROC_BROWSER_TEST_F(UserCloudPolicyManagerTest, StartSession) {
  const char* const kStartupURLs[] = {"chrome://policy", "chrome://about"};

  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword);

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  const int expected_tab_count = static_cast<int>(arraysize(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i) {
    EXPECT_EQ(GURL(kStartupURLs[i]),
              tabs->GetWebContentsAt(i)->GetVisibleURL());
  }
}

// Test disabled. See crbug.com/600617.
IN_PROC_BROWSER_TEST_F(UserCloudPolicyManagerTest,
                       DISABLED_ErrorLoadingPolicy) {
  // Delete the policy file - this will cause a 500 error on policy requests.
  user_policy_helper()->DeletePolicyFile();
  SkipToLoginScreen();
  CountNotificationObserver observer(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  GetLoginDisplay()->ShowSigninScreenForCreds(kAccountId, kAccountPassword);
  base::RunLoop().Run();
  // Should not receive a SESSION_STARTED notification.
  ASSERT_EQ(0, observer.notification_count());

  // User should not be marked as having a valid OAuth token. That way, if we
  // try to load the user in the future, we will attempt to load policy again.
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  EXPECT_NE(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());
}

}  // namespace policy
