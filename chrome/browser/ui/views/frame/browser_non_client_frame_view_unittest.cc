// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

class BrowserNonClientFrameViewTest : public TestWithBrowserView {
 public:
  BrowserNonClientFrameViewTest()
      : TestWithBrowserView(Browser::TYPE_POPUP, false), frame_view_(nullptr) {}

  // TestWithBrowserView override:
  void SetUp() override {
#if defined(OS_WIN)
    // Use opaque frame.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableDwmComposition);
#endif
    TestWithBrowserView::SetUp();
    views::Widget* widget = browser_view()->GetWidget();
    frame_view_ = static_cast<BrowserNonClientFrameView*>(
        widget->non_client_view()->frame_view());
  }

 protected:
  // Owned by the browser view.
  BrowserNonClientFrameView* frame_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewTest);
};

TEST_F(BrowserNonClientFrameViewTest, HitTestTopChrome) {
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(-1, 4, 1, 1)));
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(4, -1, 1, 1)));
  const int top_inset = frame_view_->GetTopInset(false);
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(4, top_inset, 1, 1)));
  if (top_inset > 0)
    EXPECT_TRUE(frame_view_->HitTestRect(gfx::Rect(4, top_inset - 1, 1, 1)));
}
