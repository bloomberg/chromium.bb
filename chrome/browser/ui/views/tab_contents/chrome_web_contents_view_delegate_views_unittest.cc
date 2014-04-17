// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"

class ChromeWebContentsViewDelegateViewsTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeWebContentsViewDelegateViewsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewDelegateViewsTest);
};

// Test that makes sure ShowContextMenu does not crash if web_contents() does
// not have a focused frame.
TEST_F(ChromeWebContentsViewDelegateViewsTest, ContextMenuNoFocusedFrame) {
  scoped_ptr<ChromeWebContentsViewDelegateViews> delegate_view(
      new ChromeWebContentsViewDelegateViews(web_contents()));
  EXPECT_FALSE(web_contents()->GetFocusedFrame());
  const content::ContextMenuParams params;
  delegate_view->ShowContextMenu(web_contents()->GetMainFrame(), params);
}
