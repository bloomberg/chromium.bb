// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test.h"

#include "base/utf_string_conversions.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, Basic) {
  GURL url = GetTestUrl(".", "simple_page.html");

  string16 expected_title(ASCIIToUTF16("OK"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  NavigateToURL(shell(), url);
  string16 title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, title);
}

}  // namespace content
