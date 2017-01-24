// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_TEST_WM_TEST_BASE_H_
#define ASH_MUS_TEST_WM_TEST_BASE_H_

#include <memory>
#include <string>

#include "ash/test/wm_window_test_api.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/wm/public/window_types.h"

namespace aura {
class Window;
}

namespace display {
class Display;
}

namespace gfx {
class Rect;
}

namespace ui {
class ScopedAnimationDurationScaleMode;
}

namespace ash {
namespace mus {

class AshTestImplMus;
class WmTestHelper;

// Base class for window manager tests that want to configure
// WindowTreeClient without a client to mus.
// TODO(sky): nuke this.
class WmTestBase : public testing::Test {
 public:
  WmTestBase();
  ~WmTestBase() override;

  // Update the display configuration as given in |display_spec|.
  // See test::DisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_spec);

  aura::Window* GetPrimaryRootWindow();
  aura::Window* GetSecondaryRootWindow();

  display::Display GetPrimaryDisplay();
  display::Display GetSecondaryDisplay();

  // Creates a top level window visible window in the appropriate container.
  // NOTE: you can explicitly destroy the returned value if necessary, but it
  // will also be automatically destroyed when the WindowTreeClient is
  // destroyed.
  aura::Window* CreateTestWindow(const gfx::Rect& bounds);
  aura::Window* CreateTestWindow(const gfx::Rect& bounds,
                                 ui::wm::WindowType window_type);

  // Creates a visibile fullscreen top level window on the display corresponding
  // to the display_id provided.
  aura::Window* CreateFullscreenTestWindow(
      int64_t display_id = display::kInvalidDisplayId);

  // Creates a window parented to |parent|. The returned window is visible.
  aura::Window* CreateChildTestWindow(aura::Window* parent,
                                      const gfx::Rect& bounds);

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  friend class AshTestImplMus;

  // Simulates the first user logging in and the session becoming active.
  // Classic ash handles this via AshTestHelper and TestSessionStateDelegate.
  void SimulateUserLogin();

  bool setup_called_ = false;
  bool teardown_called_ = false;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  std::unique_ptr<WmWindowTestApi::GlobalMinimumSizeLock> minimum_size_lock_;
  std::unique_ptr<WmTestHelper> test_helper_;

  DISALLOW_COPY_AND_ASSIGN(WmTestBase);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_TEST_WM_TEST_BASE_H_
