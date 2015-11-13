// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"

#include <stdint.h>

#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

class DataUseUITabModelTest : public ChromeRenderViewHostTestHarness {
 public:
  using ChromeRenderViewHostTestHarness::web_contents;

  DataUseUITabModel* data_use_ui_tab_model() {
    return chrome::android::DataUseUITabModelFactory::GetForBrowserContext(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  }
};

// Tests that DataUseTabModel is notified of tab closure and navigation events.
TEST_F(DataUseUITabModelTest, ReportTabEventsTest) {
  data_use_ui_tab_model()->ReportBrowserNavigation(
      GURL("https://www.example.com"),
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      SessionTabHelper::IdForTab(web_contents()));
  data_use_ui_tab_model()->ReportTabClosure(
      SessionTabHelper::IdForTab(web_contents()));
  // TODO(tbansal): Test that DataUseTabModel is notified.
}

// Tests if the Entrance/Exit UI state is tracked correctly.
TEST_F(DataUseUITabModelTest, EntranceExitState) {
  int32_t foo_tab_id = 1;
  int32_t bar_tab_id = 2;
  int32_t baz_tab_id = 3;

  // ShowEntrance should return true only once.
  data_use_ui_tab_model()->OnTrackingStarted(foo_tab_id);
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(foo_tab_id));

  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(bar_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(bar_tab_id));

  // ShowExit should return true only once.
  data_use_ui_tab_model()->OnTrackingEnded(foo_tab_id);
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingEnded(foo_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(foo_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));

  // The tab enters the tracking state again.
  data_use_ui_tab_model()->OnTrackingStarted(foo_tab_id);
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));

  // The tab exits the tracking state.
  data_use_ui_tab_model()->OnTrackingEnded(foo_tab_id);
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));

  // The tab enters the tracking state again.
  data_use_ui_tab_model()->OnTrackingStarted(foo_tab_id);
  data_use_ui_tab_model()->OnTrackingStarted(foo_tab_id);
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id));

  // ShowExit should return true only once.
  data_use_ui_tab_model()->OnTrackingEnded(bar_tab_id);
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(bar_tab_id));
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingEnded(bar_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(bar_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(bar_tab_id));

  data_use_ui_tab_model()->ReportTabClosure(foo_tab_id);
  data_use_ui_tab_model()->ReportTabClosure(bar_tab_id);

  //  HasDataUseTrackingStarted/Ended should return false for closed tabs.
  data_use_ui_tab_model()->OnTrackingStarted(baz_tab_id);
  data_use_ui_tab_model()->ReportTabClosure(baz_tab_id);
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(baz_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(baz_tab_id));
}

}  // namespace android

}  // namespace chrome
