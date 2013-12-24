// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class IFrameTest : public InProcessBrowserTest {
 protected:
  void NavigateAndVerifyTitle(const char* file, const char* page_title) {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(), base::FilePath().AppendASCII(file));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(base::ASCIIToUTF16(page_title),
              browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
  }
};

IN_PROC_BROWSER_TEST_F(IFrameTest, Crash) {
  NavigateAndVerifyTitle("iframe.html", "iframe test");
}

IN_PROC_BROWSER_TEST_F(IFrameTest, InEmptyFrame) {
  NavigateAndVerifyTitle("iframe_in_empty_frame.html", "iframe test");
}
