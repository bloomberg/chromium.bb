// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ui/zoom/zoom_controller.h"

namespace {

class ZoomDecorationTest : public ChromeRenderViewHostTestHarness {};

class MockZoomDecoration : public ZoomDecoration {
 public:
  explicit MockZoomDecoration(LocationBarViewMac* owner)
      : ZoomDecoration(owner), update_ui_count_(0) {}
  bool ShouldShowDecoration() const override { return true; }
  void ShowAndUpdateUI(ui_zoom::ZoomController* zoom_controller,
                       NSString* tooltip_string) override {
    ++update_ui_count_;
    ZoomDecoration::ShowAndUpdateUI(zoom_controller, tooltip_string);
  }

  int update_ui_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockZoomDecoration);
};

class MockZoomController : public ui_zoom::ZoomController {
 public:
  explicit MockZoomController(content::WebContents* web_contents)
      : ui_zoom::ZoomController(web_contents) {}
  int GetZoomPercent() const override { return zoom_percent_; }

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
  decoration.UpdateIfNecessary(&controller, /*default_zoom_changed=*/false);
  EXPECT_EQ(1, decoration.update_ui_count_);

  decoration.UpdateIfNecessary(&controller, /*default_zoom_changed=*/false);
  EXPECT_EQ(1, decoration.update_ui_count_);

  controller.zoom_percent_ = 80;
  decoration.UpdateIfNecessary(&controller, /*default_zoom_changed=*/false);
  EXPECT_EQ(2, decoration.update_ui_count_);

  // Always redraw if the default zoom changes.
  decoration.UpdateIfNecessary(&controller, /*default_zoom_changed=*/true);
  EXPECT_EQ(3, decoration.update_ui_count_);
}

}  // namespace
