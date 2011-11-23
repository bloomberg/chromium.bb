// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

using std::wstring;

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

class LoginPromptTest : public UITest {
 protected:
  LoginPromptTest()
      : username_basic_(UTF8ToUTF16("basicuser")),
        username_digest_(UTF8ToUTF16("digestuser")),
        password_(UTF8ToUTF16("secret")),
        password_bad_(UTF8ToUTF16("denyme")),
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
  return username + UTF8ToUTF16("/") + password;
}

// Test that "Basic" HTTP authentication works.
TEST_F(LoginPromptTest, TestBasicAuth) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(UTF16ToWideHack(username_basic_),
                            UTF16ToWideHack(password_bad_)));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(UTF16ToWideHack(username_basic_),
                           UTF16ToWideHack(password_)));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_),
            WideToUTF16Hack(GetActiveTabTitle()));
}

// Test that "Digest" HTTP authentication works.
TEST_F(LoginPromptTest, TestDigestAuth) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(UTF16ToWideHack(username_digest_),
                            UTF16ToWideHack(password_bad_)));
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(UTF16ToWideHack(username_digest_),
                           UTF16ToWideHack(password_)));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_),
            WideToUTF16Hack(GetActiveTabTitle()));
}

// Test that logging in on 2 tabs at once works.
TEST_F(LoginPromptTest, TestTwoAuths) {
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
  EXPECT_TRUE(basic_tab->SetAuth(UTF16ToWideHack(username_basic_),
                                 UTF16ToWideHack(password_)));
  EXPECT_TRUE(digest_tab->NeedsAuth());
  EXPECT_TRUE(digest_tab->SetAuth(UTF16ToWideHack(username_digest_),
                                  UTF16ToWideHack(password_)));

  wstring title;
  EXPECT_TRUE(basic_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_),
            WideToUTF16Hack(title));

  EXPECT_TRUE(digest_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_),
            WideToUTF16Hack(title));
}
