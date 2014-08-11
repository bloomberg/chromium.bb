// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace {

class ZoomDecorationTest : public ChromeRenderViewHostTestHarness {};

class MockZoomDecoration : public ZoomDecoration {
 public:
  explicit MockZoomDecoration(LocationBarViewMac* owner)
      : ZoomDecoration(owner), update_ui_count_(0) {}
  virtual bool ShouldShowDecoration() const OVERRIDE { return true; }
  virtual void ShowAndUpdateUI(ZoomController* zoom_controller,
                               NSString* tooltip_string) OVERRIDE {
    ++update_ui_count_;
    ZoomDecoration::ShowAndUpdateUI(zoom_controller, tooltip_string);
  }

  int update_ui_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockZoomDecoration);
};

class MockZoomController : public ZoomController {
 public:
  explicit MockZoomController(content::WebContents* web_contents)
      : ZoomController(web_contents) {}
  virtual int GetZoomPercent() const OVERRIDE { return zoom_percent_; }

  int zoom_percent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockZoomController);
};

// Test that UpdateIfNecessary performs redraws only when the zoom percent
// changes.
TEST_F(ZoomDecorationTest, ChangeZoomPercent) {
  MockZoomDecoration decoration(NULL);
  MockZoomController controller(web_contents());

  controller.zoom_percent_ = 100;
  decoration.UpdateIfNecessary(&controller);
  EXPECT_EQ(1, decoration.update_ui_count_);

  decoration.UpdateIfNecessary(&controller);
  EXPECT_EQ(1, decoration.update_ui_count_);

  controller.zoom_percent_ = 80;
  decoration.UpdateIfNecessary(&controller);
  EXPECT_EQ(2, decoration.update_ui_count_);
}

}  // namespace
