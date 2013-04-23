// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests of BrowserViewLayout. Runs tests without constructing a BrowserView.
class BrowserViewLayoutTest : public BrowserWithTestWindowTest {
 public:
  BrowserViewLayoutTest() {}
  virtual ~BrowserViewLayoutTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    // Because we have a TestBrowserWindow, not a BrowserView, |layout_| is
    // not attached to a host view.
    layout_.reset(new BrowserViewLayout);
    infobar_container_.reset(new InfoBarContainerView(NULL, NULL));
    layout_->Init(browser(),
                  NULL,
                  infobar_container_.get(),
                  NULL,
                  NULL);
  }

  BrowserViewLayout* layout() { return layout_.get(); }

 private:
  scoped_ptr<BrowserViewLayout> layout_;
  scoped_ptr<InfoBarContainerView> infobar_container_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayoutTest);
};

// Test basic construction and initialization.
TEST_F(BrowserViewLayoutTest, BrowserViewLayout) {
  EXPECT_TRUE(layout()->browser());
  EXPECT_TRUE(layout()->GetWebContentsModalDialogHost());
  EXPECT_EQ(BrowserViewLayout::kInstantUINone, layout()->GetInstantUIState());
  EXPECT_FALSE(layout()->InfobarVisible());
  // TODO(jamescook): Add more as we break dependencies.
}

// Test the core layout functions.
TEST_F(BrowserViewLayoutTest, Layout) {
  // We don't have a bookmark bar yet, so no contents offset is required.
  EXPECT_EQ(0, layout()->GetContentsOffsetForBookmarkBar());
  EXPECT_EQ(0, layout()->GetTopMarginForActiveContent());
  EXPECT_EQ(0, layout()->GetTopMarginForOverlayContent());
  // TODO(jamescook): Add more as we break dependencies.
}
