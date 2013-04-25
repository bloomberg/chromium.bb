// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/wm/property_util.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/stringprintf.h"

namespace ash {
namespace internal {

class TouchHudTest : public test::AshTestBase {
 public:
  TouchHudTest() {}
  virtual ~TouchHudTest() {}

  virtual void SetUp() OVERRIDE {
    // Add ash-touch-hud flag to enable touch HUD. This flag should be set
    // before Ash environment is set up, i.e., before
    // test::AshTestBase::SetUp().
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshTouchHud);

    test::AshTestBase::SetUp();

    // Initialize display infos. They should be initialized after Ash
    // environment is set up, i.e., after test::AshTestBase::SetUp().
    internal_display_id_ = test::DisplayManagerTestApi(GetDisplayManager()).
        SetFirstDisplayAsInternalDisplay();
    external_display_id_ = 10;
    mirrored_display_id_ = 11;

    internal_display_info_ =
        CreateDisplayInfo(internal_display_id_, gfx::Rect(0, 0, 500, 500));
    external_display_info_ =
        CreateDisplayInfo(external_display_id_, gfx::Rect(1, 1, 100, 100));
    mirrored_display_info_ =
        CreateDisplayInfo(mirrored_display_id_, gfx::Rect(0, 0, 100, 100));
  }

  const gfx::Display& GetPrimaryDisplay() {
    return GetDisplayController()->GetPrimaryDisplay();
  }

  const gfx::Display& GetSecondaryDisplay() {
    return *GetDisplayController()->GetSecondaryDisplay();
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
    const gfx::Display& internal_display =
        GetDisplayManager()->GetDisplayForId(internal_display_id_);
    GetDisplayController()->SetPrimaryDisplay(internal_display);
  }

  void SetExternalAsPrimary() {
    const gfx::Display& external_display =
        GetDisplayManager()->GetDisplayForId(external_display_id_);
    GetDisplayController()->SetPrimaryDisplay(external_display);
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

  int64 internal_display_id() const {
    return internal_display_id_;
  }

  int64 external_display_id() const {
    return external_display_id_;
  }

  void CheckInternalDisplay() {
    EXPECT_NE(static_cast<internal::TouchObserverHUD*>(NULL),
              GetInternalTouchHud());
    EXPECT_EQ(internal_display_id(), GetInternalTouchHud()->display_id_);
    EXPECT_EQ(GetInternalRootWindow(), GetInternalTouchHud()->root_window_);
    EXPECT_EQ(GetInternalRootWindow(),
              GetInternalTouchHud()->widget_->GetNativeView()->GetRootWindow());
    EXPECT_EQ(GetInternalDisplay().size(),
              GetInternalTouchHud()->widget_->GetWindowBoundsInScreen().size());
  }

  void CheckExternalDisplay() {
    EXPECT_NE(static_cast<internal::TouchObserverHUD*>(NULL),
              GetExternalTouchHud());
    EXPECT_EQ(external_display_id(), GetExternalTouchHud()->display_id_);
    EXPECT_EQ(GetExternalRootWindow(), GetExternalTouchHud()->root_window_);
    EXPECT_EQ(GetExternalRootWindow(),
              GetExternalTouchHud()->widget_->GetNativeView()->GetRootWindow());
    EXPECT_EQ(GetExternalDisplay().size(),
              GetExternalTouchHud()->widget_->GetWindowBoundsInScreen().size());
  }

 private:
  DisplayManager* GetDisplayManager() {
    return Shell::GetInstance()->display_manager();
  }

  DisplayController* GetDisplayController() {
    return Shell::GetInstance()->display_controller();
  }

  const gfx::Display& GetInternalDisplay() {
    return GetDisplayManager()->GetDisplayForId(internal_display_id_);
  }

  const gfx::Display& GetExternalDisplay() {
    return GetDisplayManager()->GetDisplayForId(external_display_id_);
  }

  aura::RootWindow* GetInternalRootWindow() {
    return GetDisplayController()->GetRootWindowForDisplayId(
        internal_display_id_);
  }

  aura::RootWindow* GetExternalRootWindow() {
    return GetDisplayController()->GetRootWindowForDisplayId(
        external_display_id_);
  }

  aura::RootWindow* GetPrimaryRootWindow() {
    const gfx::Display& display = GetPrimaryDisplay();
    return GetDisplayController()->GetRootWindowForDisplayId(display.id());
  }

  aura::RootWindow* GetSecondaryRootWindow() {
    const gfx::Display& display = GetSecondaryDisplay();
    return GetDisplayController()->GetRootWindowForDisplayId(display.id());
  }

  internal::RootWindowController* GetInternalRootController() {
    aura::RootWindow* root = GetInternalRootWindow();
    return GetRootWindowController(root);
  }

  internal::RootWindowController* GetExternalRootController() {
    aura::RootWindow* root = GetExternalRootWindow();
    return GetRootWindowController(root);
  }

  internal::RootWindowController* GetPrimaryRootController() {
    aura::RootWindow* root = GetPrimaryRootWindow();
    return GetRootWindowController(root);
  }

  internal::RootWindowController* GetSecondaryRootController() {
    aura::RootWindow* root = GetSecondaryRootWindow();
    return GetRootWindowController(root);
  }

  internal::TouchObserverHUD* GetInternalTouchHud() {
    return GetInternalRootController()->touch_observer_hud();
  }

  internal::TouchObserverHUD* GetExternalTouchHud() {
    return GetExternalRootController()->touch_observer_hud();
  }

  internal::TouchObserverHUD* GetPrimaryTouchHud() {
    return GetPrimaryRootController()->touch_observer_hud();
  }

  internal::TouchObserverHUD* GetSecondaryTouchHud() {
    return GetSecondaryRootController()->touch_observer_hud();
  }

  DisplayInfo CreateDisplayInfo(int64 id, const gfx::Rect& bounds) {
    DisplayInfo info(id, base::StringPrintf("x-%"PRId64, id), false);
    info.SetBounds(bounds);
    return info;
  }

  int64 internal_display_id_;
  int64 external_display_id_;
  int64 mirrored_display_id_;
  DisplayInfo internal_display_info_;
  DisplayInfo external_display_info_;
  DisplayInfo mirrored_display_info_;

  std::vector<DisplayInfo> display_info_list_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudTest);
};

// Checks if touch HUDs are correctly initialized for displays.
TEST_F(TouchHudTest, Basic) {
  // Setup a dual display setting.
  SetupDualDisplays();

  // Check if touch HUDs are set correctly and associated with appropriate
  // displays.
  CheckInternalDisplay();
  CheckExternalDisplay();
}

// Checks if touch HUDs are correctly handled when primary display is changed.
TEST_F(TouchHudTest, SwapPrimaryDisplay) {
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

// Checks if touch HUDs are correctly handled when displays are mirrored.
TEST_F(TouchHudTest, MirrorDisplays) {
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

// Checks if touch HUDs are correctly handled when displays are mirrored after
// setting the external display as the primary one.
TEST_F(TouchHudTest, SwapPrimaryThenMirrorDisplays) {
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

// Checks if touch HUDs are correctly handled when the external display, which
// is the secondary one, is removed.
TEST_F(TouchHudTest, RemoveSecondaryDisplay) {
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

// Checks if touch HUDs are correctly handled when the external display, which
// is set as the primary display, is removed.
TEST_F(TouchHudTest, RemovePrimaryDisplay) {
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

// Checks if touch HUDs are correctly handled when all displays are removed.
TEST_F(TouchHudTest, Headless) {
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

}  // namespace internal
}  // namespace ash
