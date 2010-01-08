// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, IncognitoNoScript) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartHTTPServer();

  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  FilePath extension_path = test_data_dir_.AppendASCII("api_test")
      .AppendASCII("incognito_no_script");
  ASSERT_TRUE(LoadExtension(extension_path));

  // Open incognito window and navigate to test page.
  Browser::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.foo.com:1337/files/extensions/test_file.html"));
  Profile* off_the_record_profile =
      browser()->profile()->GetOffTheRecordProfile();
  Browser* otr_browser = Browser::Create(off_the_record_profile);
  otr_browser->AddTabWithURL(
      GURL("http://www.foo.com:1337/files/extensions/test_file.html"),
      GURL(),
      PageTransition::LINK,
      true,
      -1,
      false,
      NULL);
  otr_browser->window()->Show();
  ui_test_utils::WaitForNavigationInCurrentTab(otr_browser);

  string16 title;
  ui_test_utils::GetCurrentTabTitle(otr_browser, &title);
  ASSERT_EQ("Unmodified", UTF16ToASCII(title));
}
