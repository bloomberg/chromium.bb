// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model_impl.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

class MockTabLifecycleObserver : public TabLifecycleObserver {
 public:
  MockTabLifecycleObserver() = default;

  MOCK_METHOD2(OnDiscardedStateChange,
               void(content::WebContents* contents, bool is_discarded));
  MOCK_METHOD2(OnAutoDiscardableStateChange,
               void(content::WebContents* contents, bool is_auto_discardable));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabLifecycleObserver);
};

}  // namespace

class TabLifecycleUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  using TabLifecycleUnit = TabLifecycleUnitSource::TabLifecycleUnit;

  TabLifecycleUnitTest() : scoped_set_tick_clock_for_testing_(&test_clock_) {
    observers_.AddObserver(&observer_);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_contents_.reset(content::TestWebContents::Create(profile(), nullptr));
    // Commit an URL to allow discarding.
    web_contents_->SetLastCommittedURL(GURL("https://www.example.com"));

    tab_strip_model_ = std::make_unique<TabStripModelImpl>(
        &tab_strip_model_delegate_, profile());
    tab_strip_model_->AppendWebContents(web_contents_.get(), false);
    web_contents_->WasHidden();
    tab_strip_model_->AppendWebContents(web_contents(), false);
    web_contents()->WasHidden();
  }

  void TearDown() override {
    tab_strip_model_.reset();
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  testing::StrictMock<MockTabLifecycleObserver> observer_;
  base::ObserverList<TabLifecycleObserver> observers_;
  std::unique_ptr<content::TestWebContents> web_contents_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::SimpleTestTickClock test_clock_;

 private:
  TestTabStripModelDelegate tab_strip_model_delegate_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitTest);
};

TEST_F(TabLifecycleUnitTest, CanDiscardByDefault) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  test_clock_.Advance(kShortDelay);

  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

TEST_F(TabLifecycleUnitTest, SetFocused) {
  test_clock_.Advance(kShortDelay);
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  EXPECT_EQ(base::TimeTicks(),
            tab_lifecycle_unit.GetSortKey().last_focused_time);
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  test_clock_.Advance(kShortDelay);
  tab_lifecycle_unit.SetFocused(true);
  tab_strip_model_->ActivateTabAt(0, false);
  web_contents_->WasShown();
  EXPECT_EQ(base::TimeTicks::Max(),
            tab_lifecycle_unit.GetSortKey().last_focused_time);
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  test_clock_.Advance(kShortDelay);
  tab_lifecycle_unit.SetFocused(false);
  tab_strip_model_->ActivateTabAt(1, false);
  web_contents_->WasHidden();
  EXPECT_EQ(test_clock_.NowTicks(),
            tab_lifecycle_unit.GetSortKey().last_focused_time);
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
#else
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
#endif

  test_clock_.Advance(kTabFocusedProtectionTime);
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

TEST_F(TabLifecycleUnitTest, AutoDiscardable) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  EXPECT_CALL(observer_,
              OnAutoDiscardableStateChange(web_contents_.get(), false));
  tab_lifecycle_unit.SetAutoDiscardable(false);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_FALSE(tab_lifecycle_unit.IsAutoDiscardable());
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  EXPECT_CALL(observer_,
              OnAutoDiscardableStateChange(web_contents_.get(), true));
  tab_lifecycle_unit.SetAutoDiscardable(true);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

TEST_F(TabLifecycleUnitTest, CannotDiscardCrashed) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  web_contents_->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

#if !defined(OS_CHROMEOS)
TEST_F(TabLifecycleUnitTest, CannotDiscardActive) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  tab_strip_model_->ActivateTabAt(0, false);

  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(TabLifecycleUnitTest, CannotDiscardInvalidURL) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  web_contents_->SetLastCommittedURL(GURL("invalid :)"));

  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

TEST_F(TabLifecycleUnitTest, CannotDiscardEmptyURL) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  web_contents_->SetLastCommittedURL(GURL());

  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

TEST_F(TabLifecycleUnitTest, CannotDiscardVideoCapture) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());

  content::MediaStreamDevices video_devices(1);
  video_devices[0] =
      content::MediaStreamDevice(content::MEDIA_DEVICE_VIDEO_CAPTURE,
                                 "fake_media_device", "fake_media_device");
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          web_contents_.get(), video_devices);
  video_stream_ui->OnStarted(base::RepeatingClosure());

  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  video_stream_ui.reset();

  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

TEST_F(TabLifecycleUnitTest, CannotDiscardRecentlyAudible) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  test_clock_.Advance(kShortDelay);

  // Cannot discard when the "recently audible" bit is set.
  tab_lifecycle_unit.SetRecentlyAudible(true);
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  // The "recently audible" bit is still set. The tab cannot be discarded.
  test_clock_.Advance(kTabAudioProtectionTime);
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  // The "recently audible" bit was unset less than
  // kTabAudioProtectionTime ago. The tab cannot be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  test_clock_.Advance(kShortDelay);
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  // The "recently audible" bit was unset kTabAudioProtectionTime ago. The tab
  // can be discarded.
  test_clock_.Advance(kTabAudioProtectionTime - kShortDelay);
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));

  // Calling SetRecentlyAudible(false) again does not change the fact that the
  // tab can be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

TEST_F(TabLifecycleUnitTest, CannotDiscardPDF) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_.get(),
                                      tab_strip_model_.get());
  web_contents_->SetMainFrameMimeType("application/pdf");

  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive));
  EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent));
}

}  // namespace resource_coordinator
