// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test.h"

#include "content/test/content_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, Basic) {
  GURL url = GetTestUrl(("."), "simple_page.html");
  NavigateToURL(shell(), url);

  // TODO(jam): move TitleWatcher in a followup change to check title.
}

}  // namespace content
