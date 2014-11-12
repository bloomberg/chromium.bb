// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/ui/zoom/zoom_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

bool operator==(const ZoomController::ZoomChangedEventData& lhs,
                const ZoomController::ZoomChangedEventData& rhs) {
  return lhs.web_contents == rhs.web_contents &&
         lhs.old_zoom_level == rhs.old_zoom_level &&
         lhs.new_zoom_level == rhs.new_zoom_level &&
         lhs.zoom_mode == rhs.zoom_mode &&
         lhs.can_show_bubble == rhs.can_show_bubble;
}

class TestZoomObserver : public ZoomObserver {
 public:
  MOCK_METHOD1(OnZoomChanged,
               void(const ZoomController::ZoomChangedEventData&));
};

class ZoomControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    zoom_controller_.reset(new ZoomController(web_contents()));
    zoom_controller_->AddObserver(&zoom_observer_);

    // This call is needed so that the RenderViewHost reports being alive. This
    // is only important for tests that call ZoomController::SetZoomLevel().
    content::RenderViewHostTester::For(rvh())->CreateRenderView(
        base::string16(), MSG_ROUTING_NONE, MSG_ROUTING_NONE, -1, false);
  }

  void TearDown() override {
    zoom_controller_->RemoveObserver(&zoom_observer_);
    zoom_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  scoped_ptr<ZoomController> zoom_controller_;
  TestZoomObserver zoom_observer_;
};

TEST_F(ZoomControllerTest, DidNavigateMainFrame) {
  double zoom_level = zoom_controller_->GetZoomLevel();
  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents(),
      zoom_level,
      zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      false);
  EXPECT_CALL(zoom_observer_, OnZoomChanged(zoom_change_data)).Times(1);
  zoom_controller_->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                         content::FrameNavigateParams());
}

TEST_F(ZoomControllerTest, Observe_ZoomController) {
  double old_zoom_level = zoom_controller_->GetZoomLevel();
  double new_zoom_level = 110.0;

  NavigateAndCommit(GURL("about:blank"));

  ZoomController::ZoomChangedEventData zoom_change_data1(
      web_contents(),
      old_zoom_level,
      old_zoom_level,
      ZoomController::ZOOM_MODE_ISOLATED,
      true /* can_show_bubble */);
  EXPECT_CALL(zoom_observer_, OnZoomChanged(zoom_change_data1)).Times(1);

  zoom_controller_->SetZoomMode(ZoomController::ZOOM_MODE_ISOLATED);

  ZoomController::ZoomChangedEventData zoom_change_data2(
      web_contents(),
      old_zoom_level,
      new_zoom_level,
      ZoomController::ZOOM_MODE_ISOLATED,
      true /* can_show_bubble */);
  EXPECT_CALL(zoom_observer_, OnZoomChanged(zoom_change_data2)).Times(1);

  zoom_controller_->SetZoomLevel(new_zoom_level);
}
