// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests of the LayoutManger for BrowserView.
class BrowserViewLayoutTest : public BrowserWithTestWindowTest {
 public:
  BrowserViewLayoutTest() : layout_(NULL) {}
  virtual ~BrowserViewLayoutTest() {}

  virtual void SetUp() OVERRIDE {
    // BrowserWithTestWindowTest takes ownership of BrowserView via |window_|.
    BrowserView* browser_view = new BrowserView;
    set_window(browser_view);

    // Creates a Browser using |browser_view| as its BrowserWindow.
    BrowserWithTestWindowTest::SetUp();

    // Memory ownership is tricky here. BrowserView needs ownership of
    // |browser|, so BrowserWithTestWindowTest cannot continue to own it.
    Browser* browser = release_browser();
    browser_view->Init(browser);

    // BrowserView also takes ownership of |layout_|.
    layout_ = new BrowserViewLayout(browser);
    browser_view->SetLayoutManager(layout_);
  }

  BrowserViewLayout* layout() { return layout_; }

 private:
  BrowserViewLayout* layout_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayoutTest);
};

// Test basic construction and initialization.
TEST_F(BrowserViewLayoutTest, BrowserViewLayout) {
  EXPECT_TRUE(layout()->browser());
  EXPECT_TRUE(layout()->GetWebContentsModalDialogHost());
  EXPECT_EQ(BrowserViewLayout::kInstantUINone, layout()->GetInstantUIState());
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
