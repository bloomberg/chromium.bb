// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "net/test/test_server.h"

class MultipartResponseTest : public InProcessBrowserTest,
                              public content::WebContentsObserver {
 public:
  MultipartResponseTest() : did_navigate_any_frame_count_(0),
                            update_history_count_(0) {}

  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE {
    did_navigate_any_frame_count_++;
    if (params.should_update_history)
      update_history_count_++;
  }

  int did_navigate_any_frame_count_;
  int update_history_count_;
};

// Disabled:  http://crbug.com/136310
IN_PROC_BROWSER_TEST_F(MultipartResponseTest, DISABLED_SingleVisit) {
  // Make sure that visiting a multipart/x-mixed-replace site only
  // creates one entry in the visits table.
  ASSERT_TRUE(test_server()->Start());

  Observe(browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL("multipart"));

  EXPECT_EQ(ASCIIToUTF16("page 9"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
  EXPECT_EQ(1, update_history_count_);
  EXPECT_GT(did_navigate_any_frame_count_, update_history_count_);
}
