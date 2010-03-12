// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/cookie_prompt_window_controller.h"
#include "chrome/browser/cookie_modal_dialog.h"

namespace {

class CookiePromptWindowControllerTest : public CocoaTest {
};

TEST_F(CookiePromptWindowControllerTest, CreateForCookie) {
  GURL url("http://chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  scoped_ptr<CookiePromptModalDialog> dialog(
      new CookiePromptModalDialog(NULL, NULL, url, cookieLine, NULL));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  EXPECT_TRUE(controller.get());
  EXPECT_TRUE([controller.get() window]);
}

TEST_F(CookiePromptWindowControllerTest, CreateForDatabase) {
  GURL url("http://google.com");
  string16 databaseName(base::SysNSStringToUTF16(@"some database"));
  scoped_ptr<CookiePromptModalDialog> dialog(
      new CookiePromptModalDialog(NULL, NULL, url, databaseName, NULL));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  EXPECT_TRUE(controller.get());
  EXPECT_TRUE([controller.get() window]);
}

TEST_F(CookiePromptWindowControllerTest, CreateForLocalStorage) {
  GURL url("http://chromium.org");
  string16 key(base::SysNSStringToUTF16(@"key"));
  string16 value(base::SysNSStringToUTF16(@"value"));
  scoped_ptr<CookiePromptModalDialog> dialog(
      new CookiePromptModalDialog(NULL, NULL, url, key, value, NULL));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  EXPECT_TRUE(controller.get());
  EXPECT_TRUE([controller.get() window]);
}

}  // namespace
