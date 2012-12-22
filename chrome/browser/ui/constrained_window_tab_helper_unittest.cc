// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class ConstrainedWindowTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  ConstrainedWindowTabHelperTest()
      : ChromeRenderViewHostTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    ConstrainedWindowTabHelper::CreateForWebContents(web_contents());
  }

 private:
  content::TestBrowserThread ui_thread_;
};

class WebContentsModalDialogCloseTest : public ConstrainedWindow {
 public:
  explicit WebContentsModalDialogCloseTest(content::WebContents* web_contents)
      : web_contents_(web_contents) {
  }

  virtual void ShowWebContentsModalDialog() {}
  virtual void FocusWebContentsModalDialog() {}
  virtual ~WebContentsModalDialogCloseTest() {}

  virtual void CloseWebContentsModalDialog() {
    ConstrainedWindowTabHelper* constrained_window_tab_helper =
        ConstrainedWindowTabHelper::FromWebContents(web_contents_);
    constrained_window_tab_helper->WillClose(this);
    close_count++;
  }

  int close_count;
  content::WebContents* web_contents_;
};

TEST_F(ConstrainedWindowTabHelperTest, ConstrainedWindows) {
  WebContentsModalDialogCloseTest window(web_contents());
  window.close_count = 0;
  ConstrainedWindowTabHelper* constrained_window_tab_helper =
      ConstrainedWindowTabHelper::FromWebContents(web_contents());

  const int kWindowCount = 4;
  for (int i = 0; i < kWindowCount; i++)
    constrained_window_tab_helper->AddDialog(&window);
  EXPECT_EQ(window.close_count, 0);

  constrained_window_tab_helper->CloseAllDialogs();
  EXPECT_EQ(window.close_count, kWindowCount);
}
