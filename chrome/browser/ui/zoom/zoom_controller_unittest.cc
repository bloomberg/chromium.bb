// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/ui/zoom/zoom_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestZoomObserver : public ZoomObserver {
 public:
  MOCK_METHOD2(OnZoomChanged, void(content::WebContents*, bool));
};

class ZoomControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    zoom_controller_.reset(new ZoomController(web_contents()));
    zoom_controller_->set_observer(&zoom_observer_);
  }

  virtual void TearDown() OVERRIDE {
    zoom_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  scoped_ptr<ZoomController> zoom_controller_;
  TestZoomObserver zoom_observer_;
};

TEST_F(ZoomControllerTest, DidNavigateMainFrame) {
  EXPECT_CALL(zoom_observer_, OnZoomChanged(web_contents(), false)).Times(1);
  zoom_controller_->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                         content::FrameNavigateParams());
}

TEST_F(ZoomControllerTest, OnPreferenceChanged) {
  EXPECT_CALL(zoom_observer_, OnZoomChanged(web_contents(), false)).Times(1);
  profile()->GetPrefs()->SetDouble(prefs::kDefaultZoomLevel, 110.0f);
}

TEST_F(ZoomControllerTest, Observe) {
  EXPECT_CALL(zoom_observer_, OnZoomChanged(web_contents(), false)).Times(1);

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  host_zoom_map->SetZoomLevelForHost(std::string(), 110.0f);
}
