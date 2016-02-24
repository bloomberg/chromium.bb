// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "policy/policy_constants.h"

namespace policy {

class UserCloudPolicyManagerTest : public LoginPolicyTestBase {
 protected:
  UserCloudPolicyManagerTest() {}

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    scoped_ptr<base::ListValue> list(new base::ListValue);
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

}  // namespace policy
