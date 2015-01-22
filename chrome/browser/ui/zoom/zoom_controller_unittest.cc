// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ui/zoom/zoom_controller.h"
#include "components/ui/zoom/zoom_observer.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ui_zoom::ZoomController;

bool operator==(const ZoomController::ZoomChangedEventData& lhs,
                const ZoomController::ZoomChangedEventData& rhs) {
  return lhs.web_contents == rhs.web_contents &&
         lhs.old_zoom_level == rhs.old_zoom_level &&
         lhs.new_zoom_level == rhs.new_zoom_level &&
         lhs.zoom_mode == rhs.zoom_mode &&
         lhs.can_show_bubble == rhs.can_show_bubble;
}

class ZoomChangedWatcher : public ui_zoom::ZoomObserver {
 public:
  ZoomChangedWatcher(
      ZoomController* zoom_controller,
      const ZoomController::ZoomChangedEventData& expected_event_data)
      : zoom_controller_(zoom_controller),
        expected_event_data_(expected_event_data),
        message_loop_runner_(new content::MessageLoopRunner) {
    zoom_controller_->AddObserver(this);
  }
  ~ZoomChangedWatcher() override { zoom_controller_->RemoveObserver(this); }

  void Wait() { message_loop_runner_->Run(); }

  void OnZoomChanged(
      const ZoomController::ZoomChangedEventData& event_data) override {
    if (event_data == expected_event_data_)
      message_loop_runner_->Quit();
  }

 private:
  ZoomController* zoom_controller_;
  ZoomController::ZoomChangedEventData expected_event_data_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ZoomChangedWatcher);
};

class ZoomControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    zoom_controller_.reset(new ZoomController(web_contents()));

    // This call is needed so that the RenderViewHost reports being alive. This
    // is only important for tests that call ZoomController::SetZoomLevel().
    content::RenderViewHostTester::For(rvh())->CreateRenderView(
        base::string16(), MSG_ROUTING_NONE, MSG_ROUTING_NONE, -1, false);
  }

  void TearDown() override {
    zoom_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  scoped_ptr<ZoomController> zoom_controller_;
};

TEST_F(ZoomControllerTest, DidNavigateMainFrame) {
  double zoom_level = zoom_controller_->GetZoomLevel();
  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents(),
      zoom_level,
      zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      false);
  ZoomChangedWatcher zoom_change_watcher(zoom_controller_.get(),
                                         zoom_change_data);
  zoom_controller_->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                         content::FrameNavigateParams());
  zoom_change_watcher.Wait();
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

  {
    ZoomChangedWatcher zoom_change_watcher1(zoom_controller_.get(),
                                            zoom_change_data1);
    zoom_controller_->SetZoomMode(ZoomController::ZOOM_MODE_ISOLATED);
    zoom_change_watcher1.Wait();
  }

  ZoomController::ZoomChangedEventData zoom_change_data2(
      web_contents(),
      old_zoom_level,
      new_zoom_level,
      ZoomController::ZOOM_MODE_ISOLATED,
      true /* can_show_bubble */);

  {
    ZoomChangedWatcher zoom_change_watcher2(zoom_controller_.get(),
                                            zoom_change_data2);
    zoom_controller_->SetZoomLevel(new_zoom_level);
    zoom_change_watcher2.Wait();
  }
}

TEST_F(ZoomControllerTest, ObserveManualZoomCanShowBubble) {
  NavigateAndCommit(GURL("about:blank"));
  double old_zoom_level = zoom_controller_->GetZoomLevel();
  double new_zoom_level1 = old_zoom_level + 0.5;
  double new_zoom_level2 = old_zoom_level + 1.0;

  zoom_controller_->SetZoomMode(ui_zoom::ZoomController::ZOOM_MODE_MANUAL);
  // By default, the zoom controller will send 'true' for can_show_bubble.
  ZoomController::ZoomChangedEventData zoom_change_data1(
      web_contents(),
      old_zoom_level,
      new_zoom_level1,
      ZoomController::ZOOM_MODE_MANUAL,
      true /* can_show_bubble */);
  {
    ZoomChangedWatcher zoom_change_watcher1(zoom_controller_.get(),
                                            zoom_change_data1);
    zoom_controller_->SetZoomLevel(new_zoom_level1);
    zoom_change_watcher1.Wait();
  }

  // Override default and verify the subsequent event reflects this change.
  zoom_controller_->SetShowsNotificationBubble(false);
  ZoomController::ZoomChangedEventData zoom_change_data2(
      web_contents(),
      new_zoom_level1,
      new_zoom_level2,
      ZoomController::ZOOM_MODE_MANUAL,
      false /* can_show_bubble */);
  {
    ZoomChangedWatcher zoom_change_watcher2(zoom_controller_.get(),
                                            zoom_change_data2);
    zoom_controller_->SetZoomLevel(new_zoom_level2);
    zoom_change_watcher2.Wait();
  }

}
