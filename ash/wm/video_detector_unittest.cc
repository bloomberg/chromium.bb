// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/video_detector.h"

#include <deque>
#include <memory>

#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state_aura.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/window_types.h"

namespace ash {
namespace test {

// Implementation that just records video state changes.
class TestObserver : public VideoDetector::Observer {
 public:
  TestObserver() {}

  bool empty() const { return states_.empty(); }
  void reset() { states_.clear(); }

  // Pops and returns the earliest-received state.
  VideoDetector::State PopState() {
    CHECK(!states_.empty());
    VideoDetector::State first_state = states_.front();
    states_.pop_front();
    return first_state;
  }

  // VideoDetector::Observer implementation.
  void OnVideoStateChanged(VideoDetector::State state) override {
    states_.push_back(state);
  }

 private:
  // States in the order they were received.
  std::deque<VideoDetector::State> states_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class VideoDetectorTest : public AshTestBase {
 public:
  VideoDetectorTest() {}
  ~VideoDetectorTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    observer_.reset(new TestObserver);
    detector_ = Shell::GetInstance()->video_detector();
    detector_->AddObserver(observer_.get());

    now_ = base::TimeTicks::Now();
    detector_->set_now_for_test(now_);
  }

  void TearDown() override {
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

  std::unique_ptr<TestObserver> observer_;

  base::TimeTicks now_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDetectorTest);
};

TEST_F(VideoDetectorTest, Basic) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));

  // Send enough updates, but make them be too small to trigger detection.
  gfx::Rect update_region(gfx::Point(),
                          gfx::Size(VideoDetector::kMinUpdateWidth - 1,
                                    VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_TRUE(observer_->empty());

  // The inactivity timer shouldn't be running yet.
  EXPECT_FALSE(detector_->TriggerTimeoutForTest());
  EXPECT_TRUE(observer_->empty());

  // Send not-quite-enough adaquately-sized updates.
  observer_->reset();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  update_region.set_size(gfx::Size(VideoDetector::kMinUpdateWidth,
                                   VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond - 1; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_TRUE(observer_->empty());

  // We should get notified after the next update, but not in response to
  // additional updates.
  detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());
  detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_TRUE(observer_->empty());

  // Wait and check that the observer is notified about inactivity.
  AdvanceTime(base::TimeDelta::FromSeconds(VideoDetector::kVideoTimeoutMs));
  ASSERT_TRUE(detector_->TriggerTimeoutForTest());
  EXPECT_EQ(VideoDetector::State::NOT_PLAYING, observer_->PopState());

  // Spread out the frames over a longer period of time, but send enough
  // over a one-second window that the observer should be notified.
  observer_->reset();
  detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_TRUE(observer_->empty());

  AdvanceTime(base::TimeDelta::FromMilliseconds(500));
  const int kNumFrames = VideoDetector::kMinFramesPerSecond + 1;
  base::TimeDelta kInterval =
      base::TimeDelta::FromMilliseconds(1000 / kNumFrames);
  for (int i = 0; i < kNumFrames; ++i) {
    AdvanceTime(kInterval);
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  }
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());

  // Let the activity time out again.
  AdvanceTime(base::TimeDelta::FromSeconds(VideoDetector::kVideoTimeoutMs));
  ASSERT_TRUE(detector_->TriggerTimeoutForTest());
  EXPECT_EQ(VideoDetector::State::NOT_PLAYING, observer_->PopState());

  // Send updates at a slower rate and check that the observer isn't notified.
  observer_->reset();
  base::TimeDelta kSlowInterval = base::TimeDelta::FromMilliseconds(
      1000 / (VideoDetector::kMinFramesPerSecond - 2));
  for (int i = 0; i < kNumFrames; ++i) {
    AdvanceTime(kSlowInterval);
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  }
  EXPECT_TRUE(observer_->empty());
}

TEST_F(VideoDetectorTest, Shutdown) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));
  gfx::Rect update_region(gfx::Point(),
                          gfx::Size(VideoDetector::kMinUpdateWidth,
                                    VideoDetector::kMinUpdateHeight));

  // It should not detect video during the shutdown.
  Shell::GetInstance()->OnAppTerminating();
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_TRUE(observer_->empty());
}

TEST_F(VideoDetectorTest, WindowNotVisible) {
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));

  // Reparent the window to the root to make sure that visibility changes aren't
  // animated.
  Shell::GetPrimaryRootWindow()->AddChild(window.get());

  // We shouldn't report video that's played in a hidden window.
  window->Hide();
  gfx::Rect update_region(gfx::Point(),
                          gfx::Size(VideoDetector::kMinUpdateWidth,
                                    VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_TRUE(observer_->empty());

  // Make the window visible and send more updates.
  observer_->reset();
  AdvanceTime(base::TimeDelta::FromSeconds(2));
  window->Show();
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());

  AdvanceTime(base::TimeDelta::FromSeconds(VideoDetector::kVideoTimeoutMs));
  ASSERT_TRUE(detector_->TriggerTimeoutForTest());
  EXPECT_EQ(VideoDetector::State::NOT_PLAYING, observer_->PopState());

  // We also shouldn't report video in a window that's fully offscreen.
  observer_->reset();
  gfx::Rect offscreen_bounds(
      gfx::Point(Shell::GetPrimaryRootWindow()->bounds().width(), 0),
      window_bounds.size());
  window->SetBounds(offscreen_bounds);
  ASSERT_EQ(offscreen_bounds, window->bounds());
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), update_region);
  EXPECT_TRUE(observer_->empty());
}

TEST_F(VideoDetectorTest, MultipleWindows) {
  // Create two windows.
  gfx::Rect window_bounds(gfx::Point(), gfx::Size(1024, 768));
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShell(SK_ColorRED, 12345, window_bounds));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShell(SK_ColorBLUE, 23456, window_bounds));

  // Even if there's video playing in both, the observer should only receive a
  // single notification.
  gfx::Rect update_region(gfx::Point(),
                          gfx::Size(VideoDetector::kMinUpdateWidth,
                                    VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window1.get(), update_region);
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window2.get(), update_region);
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());
}

// Test that the observer receives the appropriate notification when the window
// is fullscreen.
TEST_F(VideoDetectorTest, FullscreenWindow) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1024x768,1024x768");

  const gfx::Rect kLeftBounds(gfx::Point(), gfx::Size(1024, 768));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell(SK_ColorRED, 12345, kLeftBounds));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(window_state->IsFullscreen());
  window->Focus();
  const gfx::Rect kUpdateRegion(gfx::Point(),
                                gfx::Size(VideoDetector::kMinUpdateWidth,
                                          VideoDetector::kMinUpdateHeight));
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), kUpdateRegion);
  EXPECT_EQ(VideoDetector::State::PLAYING_FULLSCREEN, observer_->PopState());

  // Make the first window non-fullscreen.
  window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_FALSE(window_state->IsFullscreen());
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());

  // Open a second, fullscreen window. Fullscreen video should still be reported
  // due to the second window being fullscreen. This avoids situations where
  // non-fullscreen video could be reported when multiple videos are playing in
  // fullscreen and non-fullscreen windows.
  const gfx::Rect kRightBounds(gfx::Point(1024, 0), gfx::Size(1024, 768));
  std::unique_ptr<aura::Window> other_window(
      CreateTestWindowInShell(SK_ColorBLUE, 6789, kRightBounds));
  wm::WindowState* other_window_state = wm::GetWindowState(other_window.get());
  other_window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(other_window_state->IsFullscreen());
  for (int i = 0; i < VideoDetector::kMinFramesPerSecond; ++i)
    detector_->OnDelegatedFrameDamage(window.get(), kUpdateRegion);
  EXPECT_EQ(VideoDetector::State::PLAYING_FULLSCREEN, observer_->PopState());

  // Make the second window non-fullscreen and check that the observer is
  // immediately notified about windowed video.
  observer_->reset();
  other_window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_FALSE(other_window_state->IsFullscreen());
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());
}

}  // namespace test
}  // namespace ash
