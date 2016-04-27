// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TouchHudTestBase : public test::AshTestBase {
 public:
  TouchHudTestBase() {}
  ~TouchHudTestBase() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();

    // Initialize display infos. They should be initialized after Ash
    // environment is set up, i.e., after test::AshTestBase::SetUp().
    internal_display_id_ =
        test::DisplayManagerTestApi().SetFirstDisplayAsInternalDisplay();
    external_display_id_ = 10;
    mirrored_display_id_ = 11;

    internal_display_info_ =
        CreateDisplayInfo(internal_display_id_, gfx::Rect(0, 0, 500, 500));
    external_display_info_ =
        CreateDisplayInfo(external_display_id_, gfx::Rect(1, 1, 100, 100));
    mirrored_display_info_ =
        CreateDisplayInfo(mirrored_display_id_, gfx::Rect(0, 0, 100, 100));
  }

  display::Display GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay();
  }

  const display::Display& GetSecondaryDisplay() {
    return ScreenUtil::GetSecondaryDisplay();
  }

  void SetupSingleDisplay() {
    display_info_list_.clear();
    display_info_list_.push_back(internal_display_info_);
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void SetupDualDisplays() {
    display_info_list_.clear();
    display_info_list_.push_back(internal_display_info_);
    display_info_list_.push_back(external_display_info_);
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void SetInternalAsPrimary() {
    GetWindowTreeHostManager()->SetPrimaryDisplayId(internal_display_id_);
  }

  void SetExternalAsPrimary() {
    GetWindowTreeHostManager()->SetPrimaryDisplayId(external_display_id_);
  }

  void MirrorDisplays() {
    DCHECK_EQ(2U, display_info_list_.size());
    DCHECK_EQ(internal_display_id_, display_info_list_[0].id());
    DCHECK_EQ(external_display_id_, display_info_list_[1].id());
    display_info_list_[1] = mirrored_display_info_;
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void UnmirrorDisplays() {
    DCHECK_EQ(2U, display_info_list_.size());
    DCHECK_EQ(internal_display_id_, display_info_list_[0].id());
    DCHECK_EQ(mirrored_display_id_, display_info_list_[1].id());
    display_info_list_[1] = external_display_info_;
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void RemoveInternalDisplay() {
    DCHECK_LT(0U, display_info_list_.size());
    DCHECK_EQ(internal_display_id_, display_info_list_[0].id());
    display_info_list_.erase(display_info_list_.begin());
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void RemoveExternalDisplay() {
    DCHECK_EQ(2U, display_info_list_.size());
    display_info_list_.pop_back();
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void AddInternalDisplay() {
    DCHECK_EQ(0U, display_info_list_.size());
    display_info_list_.push_back(internal_display_info_);
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  void AddExternalDisplay() {
    DCHECK_EQ(1U, display_info_list_.size());
    display_info_list_.push_back(external_display_info_);
    GetDisplayManager()->OnNativeDisplaysChanged(display_info_list_);
  }

  int64_t internal_display_id() const { return internal_display_id_; }

  int64_t external_display_id() const { return external_display_id_; }

 protected:
  DisplayManager* GetDisplayManager() {
    return Shell::GetInstance()->display_manager();
  }

  WindowTreeHostManager* GetWindowTreeHostManager() {
    return Shell::GetInstance()->window_tree_host_manager();
  }

  const display::Display& GetInternalDisplay() {
    return GetDisplayManager()->GetDisplayForId(internal_display_id_);
  }

  const display::Display& GetExternalDisplay() {
    return GetDisplayManager()->GetDisplayForId(external_display_id_);
  }

  aura::Window* GetInternalRootWindow() {
    return GetWindowTreeHostManager()->GetRootWindowForDisplayId(
        internal_display_id_);
  }

  aura::Window* GetExternalRootWindow() {
    return GetWindowTreeHostManager()->GetRootWindowForDisplayId(
        external_display_id_);
  }

  aura::Window* GetPrimaryRootWindow() {
    const display::Display& display = GetPrimaryDisplay();
    return GetWindowTreeHostManager()->GetRootWindowForDisplayId(display.id());
  }

  aura::Window* GetSecondaryRootWindow() {
    const display::Display& display = GetSecondaryDisplay();
    return GetWindowTreeHostManager()->GetRootWindowForDisplayId(display.id());
  }

  RootWindowController* GetInternalRootController() {
    aura::Window* root = GetInternalRootWindow();
    return GetRootWindowController(root);
  }

  RootWindowController* GetExternalRootController() {
    aura::Window* root = GetExternalRootWindow();
    return GetRootWindowController(root);
  }

  RootWindowController* GetPrimaryRootController() {
    aura::Window* root = GetPrimaryRootWindow();
    return GetRootWindowController(root);
  }

  RootWindowController* GetSecondaryRootController() {
    aura::Window* root = GetSecondaryRootWindow();
    return GetRootWindowController(root);
  }

  DisplayInfo CreateDisplayInfo(int64_t id, const gfx::Rect& bounds) {
    DisplayInfo info(id, base::StringPrintf("x-%" PRId64, id), false);
    info.SetBounds(bounds);
    return info;
  }

  aura::Window* GetRootWindowForTouchHud(TouchObserverHUD* hud) {
    return hud->root_window_;
  }

  views::Widget* GetWidgetForTouchHud(TouchObserverHUD* hud) {
    return hud->widget_;
  }

  int64_t internal_display_id_;
  int64_t external_display_id_;
  int64_t mirrored_display_id_;
  DisplayInfo internal_display_info_;
  DisplayInfo external_display_info_;
  DisplayInfo mirrored_display_info_;

  std::vector<DisplayInfo> display_info_list_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudTestBase);
};

class TouchHudDebugTest : public TouchHudTestBase {
 public:
  TouchHudDebugTest() {}
  ~TouchHudDebugTest() override {}

  void SetUp() override {
    // Add ash-touch-hud flag to enable debug touch HUD. This flag should be set
    // before Ash environment is set up, i.e., before TouchHudTestBase::SetUp().
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshTouchHud);

    TouchHudTestBase::SetUp();
  }

  void CheckInternalDisplay() {
    EXPECT_NE(static_cast<TouchObserverHUD*>(NULL), GetInternalTouchHudDebug());
    EXPECT_EQ(internal_display_id(), GetInternalTouchHudDebug()->display_id());
    EXPECT_EQ(GetInternalRootWindow(),
              GetRootWindowForTouchHud(GetInternalTouchHudDebug()));
    EXPECT_EQ(GetInternalRootWindow(),
              GetWidgetForTouchHud(GetInternalTouchHudDebug())->
                  GetNativeView()->GetRootWindow());
    EXPECT_EQ(GetInternalDisplay().size(),
              GetWidgetForTouchHud(GetInternalTouchHudDebug())->
                  GetWindowBoundsInScreen().size());
  }

  void CheckExternalDisplay() {
    EXPECT_NE(static_cast<TouchHudDebug*>(NULL), GetExternalTouchHudDebug());
    EXPECT_EQ(external_display_id(), GetExternalTouchHudDebug()->display_id());
    EXPECT_EQ(GetExternalRootWindow(),
              GetRootWindowForTouchHud(GetExternalTouchHudDebug()));
    EXPECT_EQ(GetExternalRootWindow(),
              GetWidgetForTouchHud(GetExternalTouchHudDebug())->
                  GetNativeView()->GetRootWindow());
    EXPECT_EQ(GetExternalDisplay().size(),
              GetWidgetForTouchHud(GetExternalTouchHudDebug())->
                  GetWindowBoundsInScreen().size());
  }

 private:
  TouchHudDebug* GetInternalTouchHudDebug() {
    return GetInternalRootController()->touch_hud_debug();
  }

  TouchHudDebug* GetExternalTouchHudDebug() {
    return GetExternalRootController()->touch_hud_debug();
  }

  TouchHudDebug* GetPrimaryTouchHudDebug() {
    return GetPrimaryRootController()->touch_hud_debug();
  }

  TouchHudDebug* GetSecondaryTouchHudDebug() {
    return GetSecondaryRootController()->touch_hud_debug();
  }

  DISALLOW_COPY_AND_ASSIGN(TouchHudDebugTest);
};

class TouchHudProjectionTest : public TouchHudTestBase {
 public:
  TouchHudProjectionTest() {}
  ~TouchHudProjectionTest() override {}

  void EnableTouchHudProjection() {
    Shell::GetInstance()->SetTouchHudProjectionEnabled(true);
  }

  void DisableTouchHudProjection() {
    Shell::GetInstance()->SetTouchHudProjectionEnabled(false);
  }

  TouchHudProjection* GetInternalTouchHudProjection() {
    return GetInternalRootController()->touch_hud_projection();
  }

  int GetInternalTouchPointsCount() {
    return GetInternalTouchHudProjection()->points_.size();
  }

  void SendTouchEventToInternalHud(ui::EventType type,
                                   const gfx::Point& location,
                                   int touch_id) {
    ui::TouchEvent event(type, location, touch_id, event_time);
    GetInternalTouchHudProjection()->OnTouchEvent(&event);

    // Advance time for next event.
    event_time += base::TimeDelta::FromMilliseconds(100);
  }

 private:
  base::TimeDelta event_time;

  DISALLOW_COPY_AND_ASSIGN(TouchHudProjectionTest);
};

// Checks if debug touch HUD is correctly initialized for a single display.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_SingleDisplay DISABLED_SingleDisplay
#else
#define MAYBE_SingleDisplay SingleDisplay
#endif
TEST_F(TouchHudDebugTest, MAYBE_SingleDisplay) {
  // Setup a single display setting.
  SetupSingleDisplay();

  // Check if touch HUD is set correctly and associated with appropriate
  // display.
  CheckInternalDisplay();
}

// Checks if debug touch HUDs are correctly initialized for two displays.
TEST_F(TouchHudDebugTest, DualDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // Setup a dual display setting.
  SetupDualDisplays();

  // Check if touch HUDs are set correctly and associated with appropriate
  // displays.
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when primary display is
// changed.
TEST_F(TouchHudDebugTest, SwapPrimaryDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Setup a dual display setting.
  SetupDualDisplays();

  // Set the primary display to the external one.
  SetExternalAsPrimary();

  // Check if displays' touch HUDs are not swapped as root windows are.
  EXPECT_EQ(external_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(internal_display_id(), GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();

  // Set the primary display back to the internal one.
  SetInternalAsPrimary();

  // Check if displays' touch HUDs are not swapped back as root windows are.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(external_display_id(), GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when displays are mirrored.
TEST_F(TouchHudDebugTest, MirrorDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // Setup a dual display setting.
  SetupDualDisplays();

  // Mirror displays.
  MirrorDisplays();

  // Check if the internal display is intact.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Unmirror displays.
  UnmirrorDisplays();

  // Check if external display is added back correctly.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(external_display_id(), GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when displays are mirrored
// after setting the external display as the primary one.
TEST_F(TouchHudDebugTest, SwapPrimaryThenMirrorDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // Setup a dual display setting.
  SetupDualDisplays();

  // Set the primary display to the external one.
  SetExternalAsPrimary();

  // Mirror displays.
  MirrorDisplays();

  // Check if the internal display is set as the primary one.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Unmirror displays.
  UnmirrorDisplays();

  // Check if the external display is added back as the primary display and
  // touch HUDs are set correctly.
  EXPECT_EQ(external_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(internal_display_id(), GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when the external display,
// which is the secondary one, is removed.
TEST_F(TouchHudDebugTest, RemoveSecondaryDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Setup a dual display setting.
  SetupDualDisplays();

  // Remove external display which is the secondary one.
  RemoveExternalDisplay();

  // Check if the internal display is intact.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Add external display back.
  AddExternalDisplay();

  // Check if displays' touch HUDs are set correctly.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(external_display_id(), GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when the external display,
// which is set as the primary display, is removed.
TEST_F(TouchHudDebugTest, RemovePrimaryDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Setup a dual display setting.
  SetupDualDisplays();

  // Set the primary display to the external one.
  SetExternalAsPrimary();

  // Remove the external display which is the primary display.
  RemoveExternalDisplay();

  // Check if the internal display is set as the primary one.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();

  // Add the external display back.
  AddExternalDisplay();

  // Check if the external display is set as primary and touch HUDs are set
  // correctly.
  EXPECT_EQ(external_display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(internal_display_id(), GetSecondaryDisplay().id());
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if debug touch HUDs are correctly handled when all displays are
// removed.
TEST_F(TouchHudDebugTest, Headless) {
  if (!SupportsMultipleDisplays())
    return;

  // Setup a single display setting.
  SetupSingleDisplay();

  // Remove the only display which is the internal one.
  RemoveInternalDisplay();

  // Add the internal display back.
  AddInternalDisplay();

  // Check if the display's touch HUD is set correctly.
  EXPECT_EQ(internal_display_id(), GetPrimaryDisplay().id());
  CheckInternalDisplay();
}

// Checks projection touch HUD with a sequence of touch-pressed, touch-moved,
// and touch-released events.
// Test if the WM sets correct work area under different density.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_TouchMoveRelease DISABLED_TouchMoveRelease
#else
#define MAYBE_TouchMoveRelease TouchMoveRelease
#endif
TEST_F(TouchHudProjectionTest, MAYBE_TouchMoveRelease) {
  SetupSingleDisplay();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());

  EnableTouchHudProjection();
  EXPECT_NE(static_cast<TouchHudProjection*>(NULL),
            GetInternalTouchHudProjection());
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_MOVED, gfx::Point(10, 20), 1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_RELEASED, gfx::Point(10, 20), 1);
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  // Disabling projection touch HUD shoud remove it without crashing.
  DisableTouchHudProjection();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());
}

// Checks projection touch HUD with a sequence of touch-pressed, touch-moved,
// and touch-cancelled events.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_TouchMoveCancel DISABLED_TouchMoveCancel
#else
#define MAYBE_TouchMoveCancel TouchMoTouchMoveCancelveRelease
#endif
TEST_F(TouchHudProjectionTest, MAYBE_TouchMoveCancel) {
  SetupSingleDisplay();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());

  EnableTouchHudProjection();
  EXPECT_NE(static_cast<TouchHudProjection*>(NULL),
            GetInternalTouchHudProjection());
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_MOVED, gfx::Point(10, 20), 1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_CANCELLED, gfx::Point(10, 20), 1);
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  // Disabling projection touch HUD shoud remove it without crashing.
  DisableTouchHudProjection();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());
}

// Checks projection touch HUD with two simultaneous touches.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_DoubleTouch DISABLED_DoubleTouch
#else
#define MAYBE_DoubleTouch DoubleTouch
#endif
TEST_F(TouchHudProjectionTest, MAYBE_DoubleTouch) {
  SetupSingleDisplay();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());

  EnableTouchHudProjection();
  EXPECT_NE(static_cast<TouchHudProjection*>(NULL),
            GetInternalTouchHudProjection());
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_PRESSED, gfx::Point(20, 10), 2);
  EXPECT_EQ(2, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_MOVED, gfx::Point(10, 20), 1);
  EXPECT_EQ(2, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_MOVED, gfx::Point(20, 20), 2);
  EXPECT_EQ(2, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_RELEASED, gfx::Point(10, 20), 1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), 2);
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  // Disabling projection touch HUD shoud remove it without crashing.
  DisableTouchHudProjection();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());
}

// Checks if turning off touch HUD projection while touching the screen is
// handled correctly.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_DisableWhileTouching DISABLED_DisableWhileTouching
#else
#define MAYBE_DisableWhileTouching DisableWhileTouching
#endif
TEST_F(TouchHudProjectionTest, MAYBE_DisableWhileTouching) {
  SetupSingleDisplay();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());

  EnableTouchHudProjection();
  EXPECT_NE(static_cast<TouchHudProjection*>(NULL),
            GetInternalTouchHudProjection());
  EXPECT_EQ(0, GetInternalTouchPointsCount());

  SendTouchEventToInternalHud(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 1);
  EXPECT_EQ(1, GetInternalTouchPointsCount());

  // Disabling projection touch HUD shoud remove it without crashing.
  DisableTouchHudProjection();
  EXPECT_EQ(NULL, GetInternalTouchHudProjection());
}

}  // namespace ash
