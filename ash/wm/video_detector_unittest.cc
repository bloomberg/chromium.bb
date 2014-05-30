// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/video_detector.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/rect.h"
#include "ui/wm/public/window_types.h"

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
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));

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

  // Spread out the frames over a longer period of time, but send enough
  // over a one-second window that the observer should be notified.
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(0, observer_->num_invocations());

  AdvanceTime(base::TimeDelta::FromMilliseconds(500));
  const int kNumFrames = VideoDetector::kMinFramesPerSecond + 1;
  base::TimeDelta kInterval =
      base::TimeDelta::FromMilliseconds(1000 / kNumFrames);
  for (int i = 0; i < kNumFrames; ++i) {
    AdvanceTime(kInterval);
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  }
  EXPECT_EQ(1, observer_->num_invocations());

  // Keep going and check that the observer is notified again.
  for (int i = 0; i < kNumFrames; ++i) {
    AdvanceTime(kInterval);
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  }
  EXPECT_EQ(2, observer_->num_invocations());

  // Send updates at a slower rate and check that the observer isn't notified.
  base::TimeDelta kSlowInterval = base::TimeDelta::FromMilliseconds(
      1000 / (VideoDetector::kMinFramesPerSecond - 2));
  for (int i = 0; i < kNumFrames; ++i) {
    AdvanceTime(kSlowInterval);
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  }
  EXPECT_EQ(2, observer_->num_invocations());
}

TEST_F(VideoDetectorTest, Shutdown) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));
  gfx::Rect update_region(
      gfx::Point(),
      gfx::Size(VideoDetector::kMinUpdateWidth,
                VideoDetector::kMinUpdateHeight));

  // It should not detect video during the shutdown.
  Shell::GetInstance()->OnAppTerminating();
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), update_region);
  EXPECT_EQ(0, observer_->num_invocations());
}

TEST_F(VideoDetectorTest, WindowNotVisible) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));

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
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));
  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShell(SK_ColorBLUE, 23456, window_bounds));

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
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));

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
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1024x768,1024x768");

  const gfx::Rect kLeftBounds(gfx::Point(), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShell(SK_ColorRED, 12345, kLeftBounds));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(window_state->IsFullscreen());
  window->Focus();
  const gfx::Rect kUpdateRegion(
      gfx::Point(),
      gfx::Size(VideoDetector::kMinUpdateWidth,
                VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), kUpdateRegion);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(1, observer_->num_fullscreens());
  EXPECT_EQ(0, observer_->num_not_fullscreens());

  // Make the first window non-fullscreen and open a second fullscreen window on
  // a different desktop.
  window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_FALSE(window_state->IsFullscreen());
  const gfx::Rect kRightBounds(gfx::Point(1024, 0), gfx::Size(1024, 768));
  scoped_ptr<aura::Window> other_window(
      CreateTestWindowInShell(SK_ColorBLUE, 6789, kRightBounds));
  wm::WindowState* other_window_state = wm::GetWindowState(other_window.get());
  other_window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(other_window_state->IsFullscreen());

  // When video is detected in the first (now non-fullscreen) window, fullscreen
  // video should still be reported due to the second window being fullscreen.
  // This avoids situations where non-fullscreen video could be reported when
  // multiple videos are playing in fullscreen and non-fullscreen windows.
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), kUpdateRegion);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(1, observer_->num_fullscreens());
  EXPECT_EQ(0, observer_->num_not_fullscreens());

  // Make the second window non-fullscreen and check that the next video report
  // is non-fullscreen.
  other_window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_FALSE(other_window_state->IsFullscreen());
  observer_->reset_stats();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnWindowPaintScheduled(window.get(), kUpdateRegion);
  EXPECT_EQ(1, observer_->num_invocations());
  EXPECT_EQ(0, observer_->num_fullscreens());
  EXPECT_EQ(1, observer_->num_not_fullscreens());
}

}  // namespace test
}  // namespace ash
