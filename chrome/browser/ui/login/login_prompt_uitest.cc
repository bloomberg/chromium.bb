// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

class LoginPromptTest : public UITest {
 protected:
  LoginPromptTest()
      : username_basic_(ASCIIToUTF16("basicuser")),
        username_digest_(ASCIIToUTF16("digestuser")),
        password_(ASCIIToUTF16("secret")),
        password_bad_(ASCIIToUTF16("denyme")),
        test_server_(net::TestServer::TYPE_HTTP, FilePath(kDocRoot)) {
  }

  void AppendTab(const GURL& url) {
    scoped_refptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(window_proxy.get());
    ASSERT_TRUE(window_proxy->AppendTab(url));
  }

 protected:
  string16 username_basic_;
  string16 username_digest_;
  string16 password_;
  string16 password_bad_;

  net::TestServer test_server_;
};

string16 ExpectedTitleFromAuth(const string16& username,
                               const string16& password) {
  // The TestServer sets the title to username/password on successful login.
  return username + char16('/') + password;
}

#if defined(OS_WIN)
// Probably related to test server flakiness in http://crbug.com/60937
#define MAYBE_TestBasicAuth FLAKY_TestBasicAuth
#else
#define MAYBE_TestBasicAuth TestBasicAuth
#endif

// Test that "Basic" HTTP authentication works.
TEST_F(LoginPromptTest, MAYBE_TestBasicAuth) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(username_basic_, password_bad_));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(username_basic_, password_));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_),
            WideToUTF16Hack(GetActiveTabTitle()));
}

#if defined(OS_WIN)
// Probably related to test server flakiness in http://crbug.com/60937
#define MAYBE_TestDigestAuth FLAKY_TestDigestAuth
#else
#define MAYBE_TestDigestAuth TestDigestAuth
#endif

// Test that "Digest" HTTP authentication works.
TEST_F(LoginPromptTest, MAYBE_TestDigestAuth) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(username_digest_, password_bad_));
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(username_digest_, password_));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_),
            WideToUTF16Hack(GetActiveTabTitle()));
}

#if defined(OS_WIN)
// Probably related to test server flakiness in http://crbug.com/60937
#define MAYBE_TestTwoAuths FLAKY_TestTwoAuths
#else
#define MAYBE_TestTwoAuths TestTwoAuths
#endif

// Test that logging in on 2 tabs at once works.
TEST_F(LoginPromptTest, MAYBE_TestTwoAuths) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> basic_tab(GetActiveTab());
  ASSERT_TRUE(basic_tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  AppendTab(GURL(chrome::kAboutBlankURL));
  scoped_refptr<TabProxy> digest_tab(GetActiveTab());
  ASSERT_TRUE(digest_tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            digest_tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(basic_tab->NeedsAuth());
  EXPECT_TRUE(basic_tab->SetAuth(username_basic_, password_));
  EXPECT_TRUE(digest_tab->NeedsAuth());
  EXPECT_TRUE(digest_tab->SetAuth(username_digest_, password_));

  string16 title;
  EXPECT_TRUE(basic_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_), title);

  EXPECT_TRUE(digest_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_), title);
}

#if defined(OS_WIN)
// Probably related to test server flakiness in http://crbug.com/60937
#define MAYBE_TestCancelAuth FLAKY_TestCancelAuth
#else
#define MAYBE_TestCancelAuth TestCancelAuth
#endif

// Test that cancelling authentication works.
TEST_F(LoginPromptTest, MAYBE_TestCancelAuth) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  // First navigate to a test server page so we have something to go back to.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(test_server_.GetURL("a")));

  // Navigating while auth is requested is the same as cancelling.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));
  EXPECT_TRUE(tab->NeedsAuth());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(test_server_.GetURL("b")));
  EXPECT_FALSE(tab->NeedsAuth());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->GoBack());  // should bring us back to 'a'
  EXPECT_FALSE(tab->NeedsAuth());

  // Now add a page and go back, so we have something to go forward to.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(test_server_.GetURL("c")));
  EXPECT_TRUE(tab->GoBack());  // should bring us back to 'a'

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->GoForward());  // should bring us to 'c'
  EXPECT_FALSE(tab->NeedsAuth());

  // Now test that cancelling works as expected.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_FALSE(tab->NeedsAuth());
  EXPECT_EQ(L"Denied: no auth", GetActiveTabTitle());
}

#if defined(OS_WIN)
// Probably related to test server flakiness in http://crbug.com/60937
#define MAYBE_SupplyRedundantAuths FLAKY_SupplyRedundantAuths
#else
#define MAYBE_SupplyRedundantAuths SupplyRedundantAuths
#endif

// If multiple tabs are looking for the same auth, the user should only have to
// enter it once.
TEST_F(LoginPromptTest, MAYBE_SupplyRedundantAuths) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> basic_tab1(GetActiveTab());
  ASSERT_TRUE(basic_tab1.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab1->NavigateToURL(test_server_.GetURL("auth-basic/1")));
  EXPECT_TRUE(basic_tab1->NeedsAuth());

  AppendTab(GURL(chrome::kAboutBlankURL));
  scoped_refptr<TabProxy> basic_tab2(GetActiveTab());
  ASSERT_TRUE(basic_tab2.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab2->NavigateToURL(test_server_.GetURL("auth-basic/2")));
  EXPECT_TRUE(basic_tab2->NeedsAuth());

  // Set the auth in only one of the tabs (but wait for the other to load).
  int64 last_navigation_time;
  ASSERT_TRUE(basic_tab2->GetLastNavigationTime(&last_navigation_time));
  EXPECT_TRUE(basic_tab1->SetAuth(username_basic_, password_));
  EXPECT_TRUE(basic_tab2->WaitForNavigation(last_navigation_time));

  // Now both tabs have loaded.
  string16 title1;
  EXPECT_TRUE(basic_tab1->GetTabTitle(&title1));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_), title1);
  string16 title2;
  EXPECT_TRUE(basic_tab2->GetTabTitle(&title2));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_), title2);
}

#if defined(OS_WIN)
// Probably related to test server flakiness in http://crbug.com/60937
#define MAYBE_CancelRedundantAuths FLAKY_CancelRedundantAuths
#else
#define MAYBE_CancelRedundantAuths CancelRedundantAuths
#endif

// If multiple tabs are looking for the same auth, and one is cancelled, the
// other should be cancelled as well.
TEST_F(LoginPromptTest, MAYBE_CancelRedundantAuths) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> basic_tab1(GetActiveTab());
  ASSERT_TRUE(basic_tab1.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab1->NavigateToURL(test_server_.GetURL("auth-basic/1")));
  EXPECT_TRUE(basic_tab1->NeedsAuth());

  AppendTab(GURL(chrome::kAboutBlankURL));
  scoped_refptr<TabProxy> basic_tab2(GetActiveTab());
  ASSERT_TRUE(basic_tab2.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab2->NavigateToURL(test_server_.GetURL("auth-basic/2")));
  EXPECT_TRUE(basic_tab2->NeedsAuth());

  // Cancel the auth in only one of the tabs (but wait for the other to load).
  int64 last_navigation_time;
  ASSERT_TRUE(basic_tab2->GetLastNavigationTime(&last_navigation_time));
  EXPECT_TRUE(basic_tab1->CancelAuth());
  EXPECT_TRUE(basic_tab2->WaitForNavigation(last_navigation_time));

  // Now both tabs have been denied.
  string16 title1;
  EXPECT_TRUE(basic_tab1->GetTabTitle(&title1));
  EXPECT_EQ(ASCIIToUTF16("Denied: no auth"), title1);
  string16 title2;
  EXPECT_TRUE(basic_tab2->GetTabTitle(&title2));
  EXPECT_EQ(ASCIIToUTF16("Denied: no auth"), title2);
}
