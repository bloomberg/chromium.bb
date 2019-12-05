// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/urls.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

using KnownInterceptionDisclosureUITest = InProcessBrowserTest;

// Tests that the chrome://connection-monitoring-detected WebUI page shows the
// expected title and strings.
IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosureUITest, PageDisplaysStrings) {
  constexpr char kTabTitle[] = "Monitoring Detected";
  constexpr char kBodyText[] = "Your activity on the web";

  ui_test_utils::NavigateToURL(
      browser(),
      content::GetWebUIURL(
          security_interstitials::kChromeUIConnectionMonitoringDetectedHost));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(base::ASCIIToUTF16(kTabTitle), contents->GetTitle());
  EXPECT_GE(ui_test_utils::FindInPage(contents, base::ASCIIToUTF16(kBodyText),
                                      true, true, nullptr, nullptr),
            1);
}
