// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/video_detector.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace test {

// Implementation that just counts the number of times we've been told that a
// video is playing.
class TestVideoDetectorObserver : public VideoDetectorObserver {
 public:
  TestVideoDetectorObserver() : num_invocations_(0),
                                num_fullscreens_(0),
                                num_not_fullscreens_(0) {}

  int num_invocations() const { return num_invocations_; }
  int num_fullscreens() const { return num_fullscreens_; }
  int num_not_fullscreens() const { return num_not_fullscreens_; }
  void reset_stats() {
    num_invocations_ = 0;
    num_fullscreens_ = 0;
    num_not_fullscreens_ = 0;
  }

  // VideoDetectorObserver implementation.
  virtual void OnVideoDetected(bool is_fullscreen) OVERRIDE {
    num_invocations_++;
    if (is_fullscreen)
      num_fullscreens_++;
    else
      num_not_fullscreens_++;
  }

 private:
  // Number of times that OnVideoDetected() has been called.
  int num_invocations_;
  // Number of times that OnVideoDetected() has been called with is_fullscreen
  // == true.
  int num_fullscreens_;
  // Number of times that OnVideoDetected() has been called with is_fullscreen
  // == false.
  int num_not_fullscreens_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoDetectorObserver);
};

class VideoDetectorTest : public AshTestBase {
 public:
  VideoDetectorTest() {}
  virtual ~VideoDetectorTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    observer_.reset(new TestVideoDetectorObserver);
    detector_ = Shell::GetInstance()->video_detector();
    detector_->AddObserver(observer_.get());

    now_ = base::TimeTicks::Now();
    detector_->set_now_for_test(now_);
  }

  virtual void TearDown() OVERRIDE {
    detector_->RemoveObserver(observer_.get());
    AshTestBase::TearDown();
  }

 protected:
  // Move |detector_|'s idea of the current time forward by |delta|.
  void AdvanceTime(base::TimeDelta delta) {
    now_ += delta;
    detector_->set_now_for_test(now_);
  }

  VideoDetector* detector_;  // not owned

  scoped_ptr<TestVideoDetectorObserver> observer_;

  base::TimeTicks now_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDetectorTest);
};

TEST_F(VideoDetectorTest, Basic) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindow(SK_ColorRED, 12345, window_bounds, NULL));

  // Send enough updates, but make them be too small to trigger detection.
  gfx::Rect update_region(
      gfx::Point(),
      gfx::Size(VideoDetector::kMinUpdateWidth - 1,
                VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(0, observer_->num_invocations());

  // Send not-quite-enough adaquately-sized updates.
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  update_region.set_size(
      gfx::Size(VideoDetector::kMinUpdateWidth,
                VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond - 1; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(0, observer_->num_invocations());

  // We should get notified after the next update, but not in response to
  // additional updates.
  detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(0, observer_->num_fullscreens());
  EXPECT_EQ(1, observer_->num_not_fullscreens());
  detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(0, observer_->num_fullscreens());
  EXPECT_EQ(1, observer_->num_not_fullscreens());

  // Spread out the frames over two seconds; we shouldn't detect video.
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond - 1; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  AdvanceTime(base::TimeDelta::FromSeconds(1));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond - 1; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(0, observer_->num_invocations());
}

TEST_F(VideoDetectorTest, WindowNotVisible) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindow(SK_ColorRED, 12345, window_bounds, NULL));

  // Reparent the window to the root to make sure that visibility changes aren't
  // animated.
  Shell::GetPrimaryRootWindow()->AddChild(window.get());

  // We shouldn't report video that's played in a hidden window.
  window->Hide();
  gfx::Rect update_region(
      gfx::Point(),
      gfx::Size(VideoDetector::kMinUpdateWidth,
                VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(0, observer_->num_invocations());

  // Make the window visible and send more updates.
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  window->Show();
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(0, observer_->num_fullscreens());
  EXPECT_EQ(1, observer_->num_not_fullscreens());

  // We also shouldn't report video in a window that's fully offscreen.
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  gfx::Rect offscreen_bounds(
      gfx::Point(Shell::GetPrimaryRootWindow()->bounds().width(), 0),
      window_bounds.size());
  window->SetBounds(offscreen_bounds);
  ASSERT_EQ(offscreen_bounds, window->bounds());
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(0, observer_->num_invocations());
}

TEST_F(VideoDetectorTest, MultipleWindows) {
  // Create two windows.
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window1(
      aura::test::CreateTestWindow(SK_ColorRED, 12345, window_bounds, NULL));
  scoped_ptr<aura::Window> window2(
      aura::test::CreateTestWindow(SK_ColorBLUE, 23456, window_bounds, NULL));

  // Even if there's video playing in both, the observer should only receive a
  // single notification.
  gfx::Rect update_region(
      gfx::Point(),
      gfx::Size(VideoDetector::kMinUpdateWidth,
                VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window1.get(), update_region);
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window2.get(), update_region);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(0, observer_->num_fullscreens());
  EXPECT_EQ(1, observer_->num_not_fullscreens());
}

// Test that the observer receives repeated notifications.
TEST_F(VideoDetectorTest, RepeatedNotifications) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindow(SK_ColorRED, 12345, window_bounds, NULL));

  gfx::Rect update_region(
      gfx::Point(),
      gfx::Size(VideoDetector::kMinUpdateWidth,
                VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(0, observer_->num_fullscreens());
  EXPECT_EQ(1, observer_->num_not_fullscreens());
  // Let enough time pass that a second notification should be sent.
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(
      static_cast<int64>(VideoDetector::kNotifyIntervalSec + 1)));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(0, observer_->num_fullscreens());
  EXPECT_EQ(1, observer_->num_not_fullscreens());
}

// Test that the observer receives a true value when the window is fullscreen.
TEST_F(VideoDetectorTest, FullscreenWindow) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindow(SK_ColorRED, 12345, window_bounds, NULL));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window->Focus();
  gfx::Rect update_region(
      gfx::Point(),
      gfx::Size(VideoDetector::kMinUpdateWidth,
                VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(1, observer_->num_fullscreens());
  EXPECT_EQ(0, observer_->num_not_fullscreens());
}

}  // namespace test
}  // namespace ash
