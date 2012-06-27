// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/multi_display_manager.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "ui/aura/display_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display.h"

namespace ash {
namespace test {

using std::vector;
using std::string;

class MultiDisplayManagerTest : public test::AshTestBase,
                                public aura::DisplayObserver,
                                public aura::WindowObserver {
 public:
  MultiDisplayManagerTest()
      : removed_count_(0U),
        root_window_destroyed_(false) {
  }
  virtual ~MultiDisplayManagerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    display_manager()->AddObserver(this);
    Shell::GetPrimaryRootWindow()->AddObserver(this);
  }
  virtual void TearDown() OVERRIDE {
    Shell::GetPrimaryRootWindow()->RemoveObserver(this);
    display_manager()->RemoveObserver(this);
    AshTestBase::TearDown();
  }

  aura::DisplayManager* display_manager() {
    return aura::Env::GetInstance()->display_manager();
  }
  const vector<gfx::Display>& changed() const { return changed_; }
  const vector<gfx::Display>& added() const { return added_; }

  string GetCountSummary() const {
    return StringPrintf("%"PRIuS" %"PRIuS" %"PRIuS,
                        changed_.size(), added_.size(), removed_count_);
  }

  void reset() {
    changed_.clear();
    added_.clear();
    removed_count_ = 0U;
    root_window_destroyed_ = false;
  }

  bool root_window_destroyed() const {
    return root_window_destroyed_;
  }

  // aura::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE {
    changed_.push_back(display);
  }
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE {
    added_.push_back(new_display);
  }
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE {
    ++removed_count_;
  }

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(aura::Window* window) {
    ASSERT_EQ(Shell::GetPrimaryRootWindow(), window);
    root_window_destroyed_ = true;
  }

 private:
  vector<gfx::Display> changed_;
  vector<gfx::Display> added_;
  size_t removed_count_;
  bool root_window_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(MultiDisplayManagerTest);
};

TEST_F(MultiDisplayManagerTest, NativeDisplayTest) {
  aura::DisplayManager::set_use_fullscreen_host_window(true);

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Update primary and add seconary.
  UpdateDisplay("0+0-500x500,0+501-400x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 1 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0)->id(), changed()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1)->id(), added()[0].id());
  EXPECT_EQ("0,0 500x500", changed()[0].bounds().ToString());
  EXPECT_EQ("0,0 400x400", added()[0].bounds().ToString());
  EXPECT_EQ("0,501 400x400", added()[0].bounds_in_pixel().ToString());
  reset();

  // Delete secondary.
  UpdateDisplay("0+0-500x500");
  EXPECT_EQ("0 0 1", GetCountSummary());
  reset();

  // Change primary.
  UpdateDisplay("0+0-1000x600");
  EXPECT_EQ("1 0 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0)->id(), changed()[0].id());
  EXPECT_EQ("0,0 1000x600", changed()[0].bounds().ToString());
  reset();

  // Add secondary.
  UpdateDisplay("0+0-1000x600,1001+0-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 1 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(1)->id(), added()[0].id());
  EXPECT_EQ("0,0 600x400", added()[0].bounds().ToString());
  EXPECT_EQ("1001,0 600x400", added()[0].bounds_in_pixel().ToString());
  reset();

  // Secondary removed, primary changed.
  UpdateDisplay("0+0-800x300");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 0 1", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0)->id(), changed()[0].id());
  EXPECT_EQ("0,0 800x300", changed()[0].bounds().ToString());
  reset();

  // # of display can go to zero when screen is off.
  const vector<gfx::Display> empty;
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 0 0", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  // Display configuration stays the same
  EXPECT_EQ("0,0 800x300",
            display_manager()->GetDisplayAt(0)->bounds().ToString());
  reset();

  // Connect to display again
  UpdateDisplay("100+100-500x400");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 0 0", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  EXPECT_EQ("0,0 500x400", changed()[0].bounds().ToString());
  EXPECT_EQ("100,100 500x400", changed()[0].bounds_in_pixel().ToString());
  reset();

  // Go back to zero and wake up with multiple displays.
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(root_window_destroyed());
  reset();

  // Add secondary.
  UpdateDisplay("0+0-1000x600,1000+0-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 1000x600",
            display_manager()->GetDisplayAt(0)->bounds().ToString());
  EXPECT_EQ("0,0 600x400",
            display_manager()->GetDisplayAt(1)->bounds().ToString());
  EXPECT_EQ("1000,0 600x400",
            display_manager()->GetDisplayAt(1)->bounds_in_pixel().ToString());
  reset();

  aura::DisplayManager::set_use_fullscreen_host_window(false);
}

// Test in emulation mode (use_fullscreen_host_window=false)
TEST_F(MultiDisplayManagerTest, EmulatorTest) {
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  internal::MultiDisplayManager::AddRemoveDisplay();
  // Update primary and add seconary.
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
#if defined(OS_WIN)
  // TODO(oshima): Windows receives resize event for some reason.
  EXPECT_EQ("1 1 0", GetCountSummary());
#else
  EXPECT_EQ("0 1 0", GetCountSummary());
#endif
  reset();

  internal::MultiDisplayManager::CycleDisplay();
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  // Observer gets called twice in this mode because
  // it gets notified both from |OnNativeDisplayChagned|
  // and from |RootWindowObserver|, which is the consequence of
  // |SetHostSize()|.
  EXPECT_EQ("4 0 0", GetCountSummary());
  reset();

  internal::MultiDisplayManager::AddRemoveDisplay();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 0 1", GetCountSummary());
  reset();

  internal::MultiDisplayManager::CycleDisplay();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 0 0", GetCountSummary());
  reset();
}

// TODO(oshima): Device scale factor is supported on chromeos only for now.
#if defined(OS_CHROMEOS)
#define MAYBE_TestDeviceScaleOnlyChange TestDeviceScaleOnlyChange
#else
#define MAYBE_TestDeviceScaleOnlyChange DISABLED_TestDeviceScaleOnlyChange
#endif

TEST_F(MultiDisplayManagerTest, MAYBE_TestDeviceScaleOnlyChange) {
  aura::DisplayManager::set_use_fullscreen_host_window(true);
  UpdateDisplay("0+0-1000x600");
  EXPECT_EQ(1,
            Shell::GetPrimaryRootWindow()->compositor()->device_scale_factor());
  EXPECT_EQ("1000x600",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  UpdateDisplay("0+0-1000x600*2");
  EXPECT_EQ(2,
            Shell::GetPrimaryRootWindow()->compositor()->device_scale_factor());
  EXPECT_EQ("500x300",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  aura::DisplayManager::set_use_fullscreen_host_window(false);
}

}  // namespace test
}  // namespace ash
