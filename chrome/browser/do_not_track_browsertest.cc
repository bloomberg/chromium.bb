// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

typedef InProcessBrowserTest DoNotTrackTest;

// Check that the DNT header is sent when the corresponding preference is set.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, Simple) {
  ASSERT_TRUE(test_server()->Start());

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kEnableDoNotTrack, true);

  GURL url = test_server()->GetURL("echoheader?DNT");
  ui_test_utils::NavigateToURL(browser(), url);

  int matches = ui_test_utils::FindInPage(
      chrome::GetActiveWebContents(browser()),
      string16(ASCIIToUTF16("1")),
      true /* forward */, false /* match case */, NULL /* ordinal */,
      NULL /* selection_rect */);

  EXPECT_EQ(1, matches);
}

// Check that the DNT header is preserved during redirects.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, Redirect) {
  ASSERT_TRUE(test_server()->Start());

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kEnableDoNotTrack, true);

  GURL final_url = test_server()->GetURL("echoheader?DNT");
  GURL url = test_server()->GetURL(
      std::string("server-redirect?") + final_url.spec());
  ui_test_utils::NavigateToURL(browser(), url);

  int matches = ui_test_utils::FindInPage(
      chrome::GetActiveWebContents(browser()),
      string16(ASCIIToUTF16("1")),
      true /* forward */, false /* match case */, NULL /* ordinal */,
      NULL /* selection_rect */);

  EXPECT_EQ(1, matches);
}

// Check that the DOM property is set when the corresponding preference is set.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, DOMProperty) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kEnableDoNotTrack, true);

  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      chrome::GetActiveWebContents(browser())));

  std::string do_not_track;
  EXPECT_TRUE(content::ExecuteJavaScriptAndExtractString(
      chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(navigator.doNotTrack)",
      &do_not_track));
  EXPECT_EQ("1", do_not_track);

  // Reset flag and check that the changed value is propagated to the existing
  // renderer.
  prefs->SetBoolean(prefs::kEnableDoNotTrack, false);

  EXPECT_TRUE(content::ExecuteJavaScriptAndExtractString(
      chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
      "",
      "window.domAutomationController.send("
      "    navigator.doNotTrack === null ? '0' : '1')",
      &do_not_track));
  EXPECT_EQ("0", do_not_track);
}
