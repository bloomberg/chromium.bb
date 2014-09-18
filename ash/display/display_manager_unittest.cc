// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_manager.h"

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_layout_store.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/mirror_window_test_api.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/env.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"

namespace ash {

using std::vector;
using std::string;

using base::StringPrintf;

namespace {

std::string ToDisplayName(int64 id) {
  return "x-" + base::Int64ToString(id);
}

}  // namespace

class DisplayManagerTest : public test::AshTestBase,
                           public gfx::DisplayObserver,
                           public aura::WindowObserver {
 public:
  DisplayManagerTest()
      : removed_count_(0U),
        root_window_destroyed_(false) {
  }
  virtual ~DisplayManagerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    Shell::GetScreen()->AddObserver(this);
    Shell::GetPrimaryRootWindow()->AddObserver(this);
  }
  virtual void TearDown() OVERRIDE {
    Shell::GetPrimaryRootWindow()->RemoveObserver(this);
    Shell::GetScreen()->RemoveObserver(this);
    AshTestBase::TearDown();
  }

  DisplayManager* display_manager() {
    return Shell::GetInstance()->display_manager();
  }
  const vector<gfx::Display>& changed() const { return changed_; }
  const vector<gfx::Display>& added() const { return added_; }

  string GetCountSummary() const {
    return StringPrintf("%" PRIuS " %" PRIuS " %" PRIuS,
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

  const DisplayInfo& GetDisplayInfo(const gfx::Display& display) {
    return display_manager()->GetDisplayInfo(display.id());
  }

  const DisplayInfo& GetDisplayInfoAt(int index) {
    return GetDisplayInfo(display_manager()->GetDisplayAt(index));
  }

  const gfx::Display& GetDisplayForId(int64 id) {
    return display_manager()->GetDisplayForId(id);
  }

  const DisplayInfo& GetDisplayInfoForId(int64 id) {
    return GetDisplayInfo(display_manager()->GetDisplayForId(id));
  }

  // aura::DisplayObserver overrides:
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t) OVERRIDE {
    changed_.push_back(display);
  }
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE {
    added_.push_back(new_display);
  }
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE {
    ++removed_count_;
  }

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    ASSERT_EQ(Shell::GetPrimaryRootWindow(), window);
    root_window_destroyed_ = true;
  }

 private:
  vector<gfx::Display> changed_;
  vector<gfx::Display> added_;
  size_t removed_count_;
  bool root_window_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManagerTest);
};

TEST_F(DisplayManagerTest, UpdateDisplayTest) {
  if (!SupportsMultipleDisplays())
    return;

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Update primary and add seconary.
  UpdateDisplay("100+0-500x500,0+501-400x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            display_manager()->GetDisplayAt(0).bounds().ToString());

  EXPECT_EQ("1 1 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  EXPECT_EQ("0,0 500x500", changed()[0].bounds().ToString());
  // Secondary display is on right.
  EXPECT_EQ("500,0 400x400", added()[0].bounds().ToString());
  EXPECT_EQ("0,501 400x400",
            GetDisplayInfo(added()[0]).bounds_in_native().ToString());
  reset();

  // Delete secondary.
  UpdateDisplay("100+0-500x500");
  EXPECT_EQ("0 0 1", GetCountSummary());
  reset();

  // Change primary.
  UpdateDisplay("1+1-1000x600");
  EXPECT_EQ("1 0 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ("0,0 1000x600", changed()[0].bounds().ToString());
  reset();

  // Add secondary.
  UpdateDisplay("1+1-1000x600,1002+0-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 1 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  // Secondary display is on right.
  EXPECT_EQ("1000,0 600x400", added()[0].bounds().ToString());
  EXPECT_EQ("1002,0 600x400",
            GetDisplayInfo(added()[0]).bounds_in_native().ToString());
  reset();

  // Secondary removed, primary changed.
  UpdateDisplay("1+1-800x300");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 0 1", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ("0,0 800x300", changed()[0].bounds().ToString());
  reset();

  // # of display can go to zero when screen is off.
  const vector<DisplayInfo> empty;
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 0 0", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  // Display configuration stays the same
  EXPECT_EQ("0,0 800x300",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  reset();

  // Connect to display again
  UpdateDisplay("100+100-500x400");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 0 0", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  EXPECT_EQ("0,0 500x400", changed()[0].bounds().ToString());
  EXPECT_EQ("100,100 500x400",
            GetDisplayInfo(changed()[0]).bounds_in_native().ToString());
  reset();

  // Go back to zero and wake up with multiple displays.
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(root_window_destroyed());
  reset();

  // Add secondary.
  UpdateDisplay("0+0-1000x600,1000+1000-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 1000x600",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  // Secondary display is on right.
  EXPECT_EQ("1000,0 600x400",
            display_manager()->GetDisplayAt(1).bounds().ToString());
  EXPECT_EQ("1000,1000 600x400",
            GetDisplayInfoAt(1).bounds_in_native().ToString());
  reset();

  // Changing primary will update secondary as well.
  UpdateDisplay("0+0-800x600,1000+1000-600x400");
  EXPECT_EQ("2 0 0", GetCountSummary());
  reset();
  EXPECT_EQ("0,0 800x600",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  EXPECT_EQ("800,0 600x400",
            display_manager()->GetDisplayAt(1).bounds().ToString());
}

// Test in emulation mode (use_fullscreen_host_window=false)
TEST_F(DisplayManagerTest, EmulatorTest) {
  if (!SupportsMultipleDisplays())
    return;

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  display_manager()->AddRemoveDisplay();
  // Update primary and add seconary.
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 1 0", GetCountSummary());
  reset();

  display_manager()->AddRemoveDisplay();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 0 1", GetCountSummary());
  reset();

  display_manager()->AddRemoveDisplay();
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 1 0", GetCountSummary());
  reset();
}

TEST_F(DisplayManagerTest, OverscanInsetsTest) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("0+0-500x500,0+501-400x400");
  reset();
  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  const DisplayInfo& display_info1 = GetDisplayInfoAt(0);
  const DisplayInfo& display_info2 = GetDisplayInfoAt(1);
  display_manager()->SetOverscanInsets(
      display_info2.id(), gfx::Insets(13, 12, 11, 10));

  std::vector<gfx::Display> changed_displays = changed();
  EXPECT_EQ(1u, changed_displays.size());
  EXPECT_EQ(display_info2.id(), changed_displays[0].id());
  EXPECT_EQ("0,0 500x500",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  DisplayInfo updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("0,501 400x400",
            updated_display_info2.bounds_in_native().ToString());
  EXPECT_EQ("378x376",
            updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("13,12,11,10",
            updated_display_info2.overscan_insets_in_dip().ToString());
  EXPECT_EQ("500,0 378x376",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());

  // Make sure that SetOverscanInsets() is idempotent.
  display_manager()->SetOverscanInsets(display_info1.id(), gfx::Insets());
  display_manager()->SetOverscanInsets(
      display_info2.id(), gfx::Insets(13, 12, 11, 10));
  EXPECT_EQ("0,0 500x500",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("0,501 400x400",
            updated_display_info2.bounds_in_native().ToString());
  EXPECT_EQ("378x376",
            updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("13,12,11,10",
            updated_display_info2.overscan_insets_in_dip().ToString());

  display_manager()->SetOverscanInsets(
      display_info2.id(), gfx::Insets(10, 11, 12, 13));
  EXPECT_EQ("0,0 500x500",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("376x378",
            GetDisplayInfoAt(1).size_in_pixel().ToString());
  EXPECT_EQ("10,11,12,13",
            GetDisplayInfoAt(1).overscan_insets_in_dip().ToString());

  // Recreate a new 2nd display. It won't apply the overscan inset because the
  // new display has a different ID.
  UpdateDisplay("0+0-500x500");
  UpdateDisplay("0+0-500x500,0+501-400x400");
  EXPECT_EQ("0,0 500x500",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("0,501 400x400",
            GetDisplayInfoAt(1).bounds_in_native().ToString());

  // Recreate the displays with the same ID.  It should apply the overscan
  // inset.
  UpdateDisplay("0+0-500x500");
  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(display_info1);
  display_info_list.push_back(display_info2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ("1,1 500x500",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("376x378",
            updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("10,11,12,13",
            updated_display_info2.overscan_insets_in_dip().ToString());

  // HiDPI but overscan display. The specified insets size should be doubled.
  UpdateDisplay("0+0-500x500,0+501-400x400*2");
  display_manager()->SetOverscanInsets(
      display_manager()->GetDisplayAt(1).id(), gfx::Insets(4, 5, 6, 7));
  EXPECT_EQ("0,0 500x500",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("0,501 400x400",
            updated_display_info2.bounds_in_native().ToString());
  EXPECT_EQ("376x380",
            updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("4,5,6,7",
            updated_display_info2.overscan_insets_in_dip().ToString());
  EXPECT_EQ("8,10,12,14",
            updated_display_info2.GetOverscanInsetsInPixel().ToString());

  // Make sure switching primary display applies the overscan offset only once.
  ash::Shell::GetInstance()->display_controller()->SetPrimaryDisplay(
      ScreenUtil::GetSecondaryDisplay());
  EXPECT_EQ("-500,0 500x500",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("0,0 500x500",
            GetDisplayInfo(ScreenUtil::GetSecondaryDisplay()).
            bounds_in_native().ToString());
  EXPECT_EQ("0,501 400x400",
            GetDisplayInfo(Shell::GetScreen()->GetPrimaryDisplay()).
            bounds_in_native().ToString());
  EXPECT_EQ("0,0 188x190",
            Shell::GetScreen()->GetPrimaryDisplay().bounds().ToString());
}

TEST_F(DisplayManagerTest, ZeroOverscanInsets) {
  if (!SupportsMultipleDisplays())
    return;

  // Make sure the display change events is emitted for overscan inset changes.
  UpdateDisplay("0+0-500x500,0+501-400x400");
  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  int64 display2_id = display_manager()->GetDisplayAt(1).id();

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets(0, 0, 0, 0));
  EXPECT_EQ(0u, changed().size());

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets(1, 0, 0, 0));
  EXPECT_EQ(1u, changed().size());
  EXPECT_EQ(display2_id, changed()[0].id());

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets(0, 0, 0, 0));
  EXPECT_EQ(1u, changed().size());
  EXPECT_EQ(display2_id, changed()[0].id());
}

TEST_F(DisplayManagerTest, TestDeviceScaleOnlyChange) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("1000x600");
  aura::WindowTreeHost* host = Shell::GetPrimaryRootWindow()->GetHost();
  EXPECT_EQ(1, host->compositor()->device_scale_factor());
  EXPECT_EQ("1000x600",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  EXPECT_EQ("1 0 0", GetCountSummary());

  UpdateDisplay("1000x600*2");
  EXPECT_EQ(2, host->compositor()->device_scale_factor());
  EXPECT_EQ("2 0 0", GetCountSummary());
  EXPECT_EQ("500x300",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
}

DisplayInfo CreateDisplayInfo(int64 id, const gfx::Rect& bounds) {
  DisplayInfo info(id, ToDisplayName(id), false);
  info.SetBounds(bounds);
  return info;
}

TEST_F(DisplayManagerTest, TestNativeDisplaysChanged) {
  const int64 internal_display_id =
      test::DisplayManagerTestApi(display_manager()).
      SetFirstDisplayAsInternalDisplay();
  const int external_id = 10;
  const int mirror_id = 11;
  const int64 invalid_id = gfx::Display::kInvalidDisplayID;
  const DisplayInfo internal_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 500));
  const DisplayInfo external_display_info =
      CreateDisplayInfo(external_id, gfx::Rect(1, 1, 100, 100));
  const DisplayInfo mirrored_display_info =
      CreateDisplayInfo(mirror_id, gfx::Rect(0, 0, 500, 500));

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  std::string default_bounds =
      display_manager()->GetDisplayAt(0).bounds().ToString();

  std::vector<DisplayInfo> display_info_list;
  // Primary disconnected.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(default_bounds,
            display_manager()->GetDisplayAt(0).bounds().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());

  if (!SupportsMultipleDisplays())
    return;

  // External connected while primary was disconnected.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  EXPECT_EQ(invalid_id, GetDisplayForId(internal_display_id).id());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(external_id).bounds_in_native().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());
  EXPECT_EQ(external_id, Shell::GetScreen()->GetPrimaryDisplay().id());

  EXPECT_EQ(internal_display_id, gfx::Display::InternalDisplayId());

  // Primary connected, with different bounds.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(internal_display_id, Shell::GetScreen()->GetPrimaryDisplay().id());

  // This combinatino is new, so internal display becomes primary.
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(10).bounds_in_native().ToString());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));

  // Emulate suspend.
  display_info_list.clear();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(10).bounds_in_native().ToString());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));

  // External display has disconnected then resumed.
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());

  // External display was changed during suspend.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());

  // suspend...
  display_info_list.clear();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());

  // and resume with different external display.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(CreateDisplayInfo(12, gfx::Rect(1, 1, 100, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());
  EXPECT_FALSE(display_manager()->IsMirrored());

  // mirrored...
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(mirrored_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(11U, display_manager()->mirrored_display_id());
  EXPECT_TRUE(display_manager()->IsMirrored());

  // Test display name.
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));
  EXPECT_EQ("x-10", display_manager()->GetDisplayNameForId(10));
  EXPECT_EQ("x-11", display_manager()->GetDisplayNameForId(11));
  EXPECT_EQ("x-12", display_manager()->GetDisplayNameForId(12));
  // Default name for the id that doesn't exist.
  EXPECT_EQ("Display 100", display_manager()->GetDisplayNameForId(100));

  // and exit mirroring.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("500,0 100x100",
            GetDisplayForId(10).bounds().ToString());

  // Turn off internal
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(invalid_id, GetDisplayForId(internal_display_id).id());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(external_id).bounds_in_native().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());

  // Switched to another display
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(
      "0,0 500x500",
      GetDisplayInfoForId(internal_display_id).bounds_in_native().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsMirrored());
}

#if defined(OS_WIN)
// TODO(scottmg): RootWindow doesn't get resized on Windows
// Ash. http://crbug.com/247916.
#define MAYBE_TestNativeDisplaysChangedNoInternal \
        DISABLED_TestNativeDisplaysChangedNoInternal
#else
#define MAYBE_TestNativeDisplaysChangedNoInternal \
        TestNativeDisplaysChangedNoInternal
#endif

TEST_F(DisplayManagerTest, MAYBE_TestNativeDisplaysChangedNoInternal) {
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Don't change the display info if all displays are disconnected.
  std::vector<DisplayInfo> display_info_list;
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Connect another display which will become primary.
  const DisplayInfo external_display_info =
      CreateDisplayInfo(10, gfx::Rect(1, 1, 100, 100));
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(10).bounds_in_native().ToString());
  EXPECT_EQ("100x100", ash::Shell::GetPrimaryRootWindow()->GetHost()->
      GetBounds().size().ToString());
}

#if defined(OS_WIN)
// Tests that rely on the actual host size/location does not work on windows.
#define MAYBE_EnsurePointerInDisplays DISABLED_EnsurePointerInDisplays
#define MAYBE_EnsurePointerInDisplays_2ndOnLeft DISABLED_EnsurePointerInDisplays_2ndOnLeft
#else
#define MAYBE_EnsurePointerInDisplays EnsurePointerInDisplays
#define MAYBE_EnsurePointerInDisplays_2ndOnLeft EnsurePointerInDisplays_2ndOnLeft
#endif

TEST_F(DisplayManagerTest, MAYBE_EnsurePointerInDisplays) {
  UpdateDisplay("200x200,300x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator(root_windows[0]);

  // Set the initial position.
  generator.MoveMouseToInHost(350, 150);
  EXPECT_EQ("350,150", env->last_mouse_location().ToString());

  // A mouse pointer will stay in the 2nd display.
  UpdateDisplay("300x300,200x200");
  EXPECT_EQ("450,50", env->last_mouse_location().ToString());

  // A mouse pointer will be outside of displays and move to the
  // center of 2nd display.
  UpdateDisplay("300x300,100x100");
  EXPECT_EQ("350,50", env->last_mouse_location().ToString());

  // 2nd display was disconnected, and the cursor is
  // now in the 1st display.
  UpdateDisplay("400x400");
  EXPECT_EQ("50,350", env->last_mouse_location().ToString());

  // 1st display's resolution has changed, and the mouse pointer is
  // now outside. Move the mouse pointer to the center of 1st display.
  UpdateDisplay("300x300");
  EXPECT_EQ("150,150", env->last_mouse_location().ToString());

  // Move the mouse pointer to the bottom of 1st display.
  generator.MoveMouseToInHost(150, 290);
  EXPECT_EQ("150,290", env->last_mouse_location().ToString());

  // The mouse pointer is now on 2nd display.
  UpdateDisplay("300x280,200x200");
  EXPECT_EQ("450,10", env->last_mouse_location().ToString());
}

TEST_F(DisplayManagerTest, MAYBE_EnsurePointerInDisplays_2ndOnLeft) {
  // Set the 2nd display on the left.
  DisplayLayoutStore* layout_store =
      Shell::GetInstance()->display_manager()->layout_store();
  DisplayLayout layout = layout_store->default_display_layout();
  layout.position = DisplayLayout::LEFT;
  layout_store->SetDefaultDisplayLayout(layout);

  UpdateDisplay("200x200,300x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  EXPECT_EQ("-300,0 300x300",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());

  aura::Env* env = aura::Env::GetInstance();

  // Set the initial position.
  root_windows[0]->MoveCursorTo(gfx::Point(-150, 250));
  EXPECT_EQ("-150,250", env->last_mouse_location().ToString());

  // A mouse pointer will stay in 2nd display.
  UpdateDisplay("300x300,200x300");
  EXPECT_EQ("-50,150", env->last_mouse_location().ToString());

  // A mouse pointer will be outside of displays and move to the
  // center of 2nd display.
  UpdateDisplay("300x300,200x100");
  EXPECT_EQ("-100,50", env->last_mouse_location().ToString());

  // 2nd display was disconnected. Mouse pointer should move to
  // 1st display.
  UpdateDisplay("300x300");
  EXPECT_EQ("150,150", env->last_mouse_location().ToString());
}

TEST_F(DisplayManagerTest, NativeDisplaysChangedAfterPrimaryChange) {
  if (!SupportsMultipleDisplays())
    return;

  const int64 internal_display_id =
      test::DisplayManagerTestApi(display_manager()).
      SetFirstDisplayAsInternalDisplay();
  const DisplayInfo native_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 500));
  const DisplayInfo secondary_display_info =
      CreateDisplayInfo(10, gfx::Rect(1, 1, 100, 100));

  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_info_list.push_back(secondary_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("500,0 100x100", GetDisplayForId(10).bounds().ToString());

  ash::Shell::GetInstance()->display_controller()->SetPrimaryDisplay(
      GetDisplayForId(secondary_display_info.id()));
  EXPECT_EQ("-500,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("0,0 100x100", GetDisplayForId(10).bounds().ToString());

  // OnNativeDisplaysChanged may change the display bounds.  Here makes sure
  // nothing changed if the exactly same displays are specified.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ("-500,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("0,0 100x100", GetDisplayForId(10).bounds().ToString());
}

TEST_F(DisplayManagerTest, DontRememberBestResolution) {
  int display_id = 1000;
  DisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
  std::vector<DisplayMode> display_modes;
  display_modes.push_back(
      DisplayMode(gfx::Size(1000, 500), 58.0f, false, true));
  display_modes.push_back(
      DisplayMode(gfx::Size(800, 300), 59.0f, false, false));
  display_modes.push_back(
      DisplayMode(gfx::Size(400, 500), 60.0f, false, false));

  native_display_info.set_display_modes(display_modes);

  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  DisplayMode mode;
  DisplayMode expected_mode;
  expected_mode.size = gfx::Size(1000, 500);
  EXPECT_FALSE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  // Unsupported resolution.
  display_manager()->SetDisplayResolution(display_id, gfx::Size(800, 4000));
  EXPECT_FALSE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  // Supported resolution.
  display_manager()->SetDisplayResolution(display_id, gfx::Size(800, 300));
  EXPECT_TRUE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  EXPECT_EQ("800x300", mode.size.ToString());
  EXPECT_EQ(59.0f, mode.refresh_rate);
  EXPECT_FALSE(mode.native);
  expected_mode.size = gfx::Size(800, 300);
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  // Best resolution.
  display_manager()->SetDisplayResolution(display_id, gfx::Size(1000, 500));
  EXPECT_TRUE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  EXPECT_EQ("1000x500", mode.size.ToString());
  EXPECT_EQ(58.0f, mode.refresh_rate);
  EXPECT_TRUE(mode.native);
  expected_mode.size = gfx::Size(1000, 500);
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
}

TEST_F(DisplayManagerTest, ResolutionFallback) {
  int display_id = 1000;
  DisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
  std::vector<DisplayMode> display_modes;
  display_modes.push_back(
      DisplayMode(gfx::Size(1000, 500), 58.0f, false, true));
  display_modes.push_back(
      DisplayMode(gfx::Size(800, 300), 59.0f, false, false));
  display_modes.push_back(
      DisplayMode(gfx::Size(400, 500), 60.0f, false, false));

  std::vector<DisplayMode> copy = display_modes;
  native_display_info.set_display_modes(copy);

  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  {
    display_manager()->SetDisplayResolution(display_id, gfx::Size(800, 300));
    DisplayInfo new_native_display_info =
        CreateDisplayInfo(display_id, gfx::Rect(0, 0, 400, 500));
    copy = display_modes;
    new_native_display_info.set_display_modes(copy);
    std::vector<DisplayInfo> new_display_info_list;
    new_display_info_list.push_back(new_native_display_info);
    display_manager()->OnNativeDisplaysChanged(new_display_info_list);

    DisplayMode mode;
    EXPECT_TRUE(
        display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
    EXPECT_EQ("400x500", mode.size.ToString());
    EXPECT_EQ(60.0f, mode.refresh_rate);
    EXPECT_FALSE(mode.native);
  }
  {
    // Best resolution should find itself on the resolutions list.
    display_manager()->SetDisplayResolution(display_id, gfx::Size(800, 300));
    DisplayInfo new_native_display_info =
        CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
    std::vector<DisplayMode> copy = display_modes;
    new_native_display_info.set_display_modes(copy);
    std::vector<DisplayInfo> new_display_info_list;
    new_display_info_list.push_back(new_native_display_info);
    display_manager()->OnNativeDisplaysChanged(new_display_info_list);

    DisplayMode mode;
    EXPECT_TRUE(
        display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
    EXPECT_EQ("1000x500", mode.size.ToString());
    EXPECT_EQ(58.0f, mode.refresh_rate);
    EXPECT_TRUE(mode.native);
  }
}

TEST_F(DisplayManagerTest, Rotate) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("100x200/r,300x400/l");
  EXPECT_EQ("1,1 100x200",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("200x100",
            GetDisplayInfoAt(0).size_in_pixel().ToString());

  EXPECT_EQ("1,201 300x400",
            GetDisplayInfoAt(1).bounds_in_native().ToString());
  EXPECT_EQ("400x300",
            GetDisplayInfoAt(1).size_in_pixel().ToString());
  reset();
  UpdateDisplay("100x200/b,300x400");
  EXPECT_EQ("2 0 0", GetCountSummary());
  reset();

  EXPECT_EQ("1,1 100x200",
            GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("100x200",
            GetDisplayInfoAt(0).size_in_pixel().ToString());

  EXPECT_EQ("1,201 300x400",
            GetDisplayInfoAt(1).bounds_in_native().ToString());
  EXPECT_EQ("300x400",
            GetDisplayInfoAt(1).size_in_pixel().ToString());

  // Just Rotating display will change the bounds on both display.
  UpdateDisplay("100x200/l,300x400");
  EXPECT_EQ("2 0 0", GetCountSummary());
  reset();

  // Updating to the same configuration should report no changes.
  UpdateDisplay("100x200/l,300x400");
  EXPECT_EQ("0 0 0", GetCountSummary());
  reset();

  // Rotating 180 degrees should report one change.
  UpdateDisplay("100x200/r,300x400");
  EXPECT_EQ("1 0 0", GetCountSummary());
  reset();

  UpdateDisplay("200x200");
  EXPECT_EQ("1 0 1", GetCountSummary());
  reset();

  // Rotating 180 degrees should report one change.
  UpdateDisplay("200x200/u");
  EXPECT_EQ("1 0 0", GetCountSummary());
  reset();

  UpdateDisplay("200x200/l");
  EXPECT_EQ("1 0 0", GetCountSummary());
}

TEST_F(DisplayManagerTest, UIScale) {
  DisplayInfo::SetUse125DSFForUIScaling(false);

  UpdateDisplay("1280x800");
  int64 display_id = Shell::GetScreen()->GetPrimaryDisplay().id();
  display_manager()->SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.0, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());

  gfx::Display::SetInternalDisplayId(display_id);

  display_manager()->SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());

  UpdateDisplay("1366x768");
  display_manager()->SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.75f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.6f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.6f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());

  UpdateDisplay("1280x850*2");
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.5f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 1.0f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  gfx::Display display = Shell::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(2.0f, display.device_scale_factor());
  EXPECT_EQ("640x425", display.bounds().size().ToString());

  display_manager()->SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display_manager()->SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());

  display_manager()->SetDisplayUIScale(display_id, 2.0f);
  EXPECT_EQ(2.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  display = Shell::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(1.0f, display.device_scale_factor());
  EXPECT_EQ("1280x850", display.bounds().size().ToString());

  // 1.25 ui scaling on 1.25 DSF device should use 1.0 DSF
  // on screen.
  UpdateDisplay("1280x850*1.25");
  display_manager()->SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  display = Shell::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(1.0f, display.device_scale_factor());
  EXPECT_EQ("1280x850", display.bounds().size().ToString());
}

TEST_F(DisplayManagerTest, UIScaleWithDisplayMode) {
  int display_id = 1000;

  // Setup the display modes with UI-scale.
  DisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1280, 800));
  std::vector<DisplayMode> display_modes;
  const DisplayMode base_mode(gfx::Size(1280, 800), 60.0f, false, false);
  std::vector<float> scales =
      DisplayManager::GetScalesForDisplay(native_display_info);
  for (size_t i = 0; i < scales.size(); i++) {
    DisplayMode mode = base_mode;
    mode.ui_scale = scales[i];
    mode.native = (scales[i] == 1.0f);
    display_modes.push_back(mode);
  }
  native_display_info.set_display_modes(display_modes);
  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  DisplayMode expected_mode = base_mode;
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  display_manager()->SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.0, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  gfx::Display::SetInternalDisplayId(display_id);

  display_manager()->SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  expected_mode.ui_scale = 1.125f;
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  expected_mode.ui_scale = 0.8f;
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  expected_mode.ui_scale = 0.625f;
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display_manager()->SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());
  expected_mode.ui_scale = 0.5f;
  EXPECT_TRUE(expected_mode.IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
}


TEST_F(DisplayManagerTest, Use125DSFRorUIScaling) {
  int64 display_id = Shell::GetScreen()->GetPrimaryDisplay().id();
  gfx::Display::SetInternalDisplayId(display_id);
  DisplayInfo::SetUse125DSFForUIScaling(true);

  UpdateDisplay("1920x1080*1.25");
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());

  display_manager()->SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ("1536x864", GetDisplayForId(display_id).size().ToString());

  display_manager()->SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ("960x540", GetDisplayForId(display_id).size().ToString());

  display_manager()->SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ("2400x1350", GetDisplayForId(display_id).size().ToString());

  DisplayInfo::SetUse125DSFForUIScaling(false);
}

#if defined(OS_WIN)
// TODO(scottmg): RootWindow doesn't get resized on Windows
// Ash. http://crbug.com/247916.
#define MAYBE_UpdateMouseCursorAfterRotateZoom DISABLED_UpdateMouseCursorAfterRotateZoom
#else
#define MAYBE_UpdateMouseCursorAfterRotateZoom UpdateMouseCursorAfterRotateZoom
#endif

TEST_F(DisplayManagerTest, MAYBE_UpdateMouseCursorAfterRotateZoom) {
  // Make sure just rotating will not change native location.
  UpdateDisplay("300x200,200x150");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator1(root_windows[0]);
  ui::test::EventGenerator generator2(root_windows[1]);

  // Test on 1st display.
  generator1.MoveMouseToInHost(150, 50);
  EXPECT_EQ("150,50", env->last_mouse_location().ToString());
  UpdateDisplay("300x200/r,200x150");
  EXPECT_EQ("50,149", env->last_mouse_location().ToString());

  // Test on 2nd display.
  generator2.MoveMouseToInHost(50, 100);
  EXPECT_EQ("250,100", env->last_mouse_location().ToString());
  UpdateDisplay("300x200/r,200x150/l");
  EXPECT_EQ("249,50", env->last_mouse_location().ToString());

  // The native location is now outside, so move to the center
  // of closest display.
  UpdateDisplay("300x200/r,100x50/l");
  EXPECT_EQ("225,50", env->last_mouse_location().ToString());

  // Make sure just zooming will not change native location.
  UpdateDisplay("600x400*2,400x300");

  // Test on 1st display.
  generator1.MoveMouseToInHost(200, 300);
  EXPECT_EQ("100,150", env->last_mouse_location().ToString());
  UpdateDisplay("600x400*2@1.5,400x300");
  EXPECT_EQ("150,225", env->last_mouse_location().ToString());

  // Test on 2nd display.
  UpdateDisplay("600x400,400x300*2");
  generator2.MoveMouseToInHost(200, 250);
  EXPECT_EQ("700,125", env->last_mouse_location().ToString());
  UpdateDisplay("600x400,400x300*2@1.5");
  EXPECT_EQ("750,187", env->last_mouse_location().ToString());

  // The native location is now outside, so move to the
  // center of closest display.
  UpdateDisplay("600x400,400x200*2@1.5");
  EXPECT_EQ("750,75", env->last_mouse_location().ToString());
}

class TestDisplayObserver : public gfx::DisplayObserver {
 public:
  TestDisplayObserver() : changed_(false) {}
  virtual ~TestDisplayObserver() {}

  // gfx::DisplayObserver overrides:
  virtual void OnDisplayMetricsChanged(const gfx::Display&,uint32_t) OVERRIDE {}
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE {
    // Mirror window should already be delete before restoring
    // the external display.
    EXPECT_FALSE(test_api.GetHost());
    changed_ = true;
  }
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE {
    // Mirror window should not be created until the external display
    // is removed.
    EXPECT_FALSE(test_api.GetHost());
    changed_ = true;
  }

  bool changed_and_reset() {
    bool changed = changed_;
    changed_ = false;
    return changed;
  }

 private:
  test::MirrorWindowTestApi test_api;
  bool changed_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayObserver);
};

TEST_F(DisplayManagerTest, SoftwareMirroring) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x400,400x500");

  test::MirrorWindowTestApi test_api;
  EXPECT_EQ(NULL, test_api.GetHost());

  TestDisplayObserver display_observer;
  Shell::GetScreen()->AddObserver(&display_observer);

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetSecondDisplayMode(DisplayManager::MIRRORING);
  display_manager->UpdateDisplays();
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_EQ(1U, display_manager->GetNumDisplays());
  EXPECT_EQ("0,0 300x400",
            Shell::GetScreen()->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("400x500", test_api.GetHost()->GetBounds().size().ToString());
  EXPECT_EQ("300x400",
            test_api.GetHost()->window()->bounds().size().ToString());
  EXPECT_TRUE(display_manager->IsMirrored());

  display_manager->SetMirrorMode(false);
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_EQ(NULL, test_api.GetHost());
  EXPECT_EQ(2U, display_manager->GetNumDisplays());
  EXPECT_FALSE(display_manager->IsMirrored());

  // Make sure the mirror window has the pixel size of the
  // source display.
  display_manager->SetMirrorMode(true);
  EXPECT_TRUE(display_observer.changed_and_reset());

  UpdateDisplay("300x400@0.5,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("300x400",
            test_api.GetHost()->window()->bounds().size().ToString());

  UpdateDisplay("310x410*2,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("310x410",
            test_api.GetHost()->window()->bounds().size().ToString());

  UpdateDisplay("320x420/r,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("320x420",
            test_api.GetHost()->window()->bounds().size().ToString());

  UpdateDisplay("330x440/r,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("330x440",
            test_api.GetHost()->window()->bounds().size().ToString());

  // Overscan insets are ignored.
  UpdateDisplay("400x600/o,600x800/o");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("400x600",
            test_api.GetHost()->window()->bounds().size().ToString());

  Shell::GetScreen()->RemoveObserver(&display_observer);
}

#if defined(OS_CHROMEOS)
// Make sure this does not cause any crashes. See http://crbug.com/412910
// This test is limited to OS_CHROMEOS because CursorCompositingEnabled is only
// for ChromeOS.
TEST_F(DisplayManagerTest, SoftwareMirroringWithCompositingCursor) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x400,400x500");

  test::MirrorWindowTestApi test_api;
  EXPECT_EQ(NULL, test_api.GetHost());

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayInfo secondary_info = display_manager->GetDisplayInfo(
      ScreenUtil::GetSecondaryDisplay().id());

  display_manager->SetSoftwareMirroring(true);
  display_manager->UpdateDisplays();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_FALSE(root_windows[0]->Contains(test_api.GetCursorWindow()));

  Shell::GetInstance()->SetCursorCompositingEnabled(true);

  EXPECT_TRUE(root_windows[0]->Contains(test_api.GetCursorWindow()));

  // Removes the first display and keeps the second one.
  display_manager->SetSoftwareMirroring(false);
  std::vector<DisplayInfo> new_info_list;
  new_info_list.push_back(secondary_info);
  display_manager->OnNativeDisplaysChanged(new_info_list);

  root_windows = Shell::GetAllRootWindows();
  EXPECT_TRUE(root_windows[0]->Contains(test_api.GetCursorWindow()));

  Shell::GetInstance()->SetCursorCompositingEnabled(false);
}
#endif  // OS_CHROMEOS

TEST_F(DisplayManagerTest, MirroredLayout) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  UpdateDisplay("500x500,400x400");
  EXPECT_FALSE(display_manager->GetCurrentDisplayLayout().mirrored);
  EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager->num_connected_displays());

  UpdateDisplay("1+0-500x500,1+0-500x500");
  EXPECT_TRUE(display_manager->GetCurrentDisplayLayout().mirrored);
  EXPECT_EQ(1, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager->num_connected_displays());

  UpdateDisplay("500x500,500x500");
  EXPECT_FALSE(display_manager->GetCurrentDisplayLayout().mirrored);
  EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager->num_connected_displays());
}

TEST_F(DisplayManagerTest, InvertLayout) {
  EXPECT_EQ("left, 0",
            DisplayLayout(DisplayLayout::RIGHT, 0).Invert().ToString());
  EXPECT_EQ("left, -100",
            DisplayLayout(DisplayLayout::RIGHT, 100).Invert().ToString());
  EXPECT_EQ("left, 50",
            DisplayLayout(DisplayLayout::RIGHT, -50).Invert().ToString());

  EXPECT_EQ("right, 0",
            DisplayLayout(DisplayLayout::LEFT, 0).Invert().ToString());
  EXPECT_EQ("right, -90",
            DisplayLayout(DisplayLayout::LEFT, 90).Invert().ToString());
  EXPECT_EQ("right, 60",
            DisplayLayout(DisplayLayout::LEFT, -60).Invert().ToString());

  EXPECT_EQ("bottom, 0",
            DisplayLayout(DisplayLayout::TOP, 0).Invert().ToString());
  EXPECT_EQ("bottom, -80",
            DisplayLayout(DisplayLayout::TOP, 80).Invert().ToString());
  EXPECT_EQ("bottom, 70",
            DisplayLayout(DisplayLayout::TOP, -70).Invert().ToString());

  EXPECT_EQ("top, 0",
            DisplayLayout(DisplayLayout::BOTTOM, 0).Invert().ToString());
  EXPECT_EQ("top, -70",
            DisplayLayout(DisplayLayout::BOTTOM, 70).Invert().ToString());
  EXPECT_EQ("top, 80",
            DisplayLayout(DisplayLayout::BOTTOM, -80).Invert().ToString());
}

#if defined(OS_WIN)
// TODO(scottmg): RootWindow doesn't get resized on Windows
// Ash. http://crbug.com/247916.
#define MAYBE_UpdateDisplayWithHostOrigin DISABLED_UpdateDisplayWithHostOrigin
#else
#define MAYBE_UpdateDisplayWithHostOrigin UpdateDisplayWithHostOrigin
#endif

TEST_F(DisplayManagerTest, MAYBE_UpdateDisplayWithHostOrigin) {
  UpdateDisplay("100x200,300x400");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  aura::WindowTreeHost* host0 = root_windows[0]->GetHost();
  aura::WindowTreeHost* host1 = root_windows[1]->GetHost();

  EXPECT_EQ("1,1", host0->GetBounds().origin().ToString());
  EXPECT_EQ("100x200", host0->GetBounds().size().ToString());
  // UpdateDisplay set the origin if it's not set.
  EXPECT_NE("1,1", host1->GetBounds().origin().ToString());
  EXPECT_EQ("300x400", host1->GetBounds().size().ToString());

  UpdateDisplay("100x200,200+300-300x400");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("0,0", host0->GetBounds().origin().ToString());
  EXPECT_EQ("100x200", host0->GetBounds().size().ToString());
  EXPECT_EQ("200,300", host1->GetBounds().origin().ToString());
  EXPECT_EQ("300x400", host1->GetBounds().size().ToString());

  UpdateDisplay("400+500-200x300,300x400");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("400,500", host0->GetBounds().origin().ToString());
  EXPECT_EQ("200x300", host0->GetBounds().size().ToString());
  EXPECT_EQ("0,0", host1->GetBounds().origin().ToString());
  EXPECT_EQ("300x400", host1->GetBounds().size().ToString());

  UpdateDisplay("100+200-100x200,300+500-200x300");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("100,200", host0->GetBounds().origin().ToString());
  EXPECT_EQ("100x200", host0->GetBounds().size().ToString());
  EXPECT_EQ("300,500", host1->GetBounds().origin().ToString());
  EXPECT_EQ("200x300", host1->GetBounds().size().ToString());
}


class ScreenShutdownTest : public test::AshTestBase {
 public:
  ScreenShutdownTest() {
  }
  virtual ~ScreenShutdownTest() {}

  virtual void TearDown() OVERRIDE {
    gfx::Screen* orig_screen =
        gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_ALTERNATE);
    AshTestBase::TearDown();
    if (!SupportsMultipleDisplays())
      return;
    gfx::Screen* screen =
        gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_ALTERNATE);
    EXPECT_NE(orig_screen, screen);
    EXPECT_EQ(2, screen->GetNumDisplays());
    EXPECT_EQ("500x300", screen->GetPrimaryDisplay().size().ToString());
    std::vector<gfx::Display> all = screen->GetAllDisplays();
    EXPECT_EQ("500x300", all[0].size().ToString());
    EXPECT_EQ("800x400", all[1].size().ToString());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenShutdownTest);
};

TEST_F(ScreenShutdownTest, ScreenAfterShutdown) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("500x300,800x400");
}


#if defined(OS_CHROMEOS)
namespace {

// A helper class that sets the display configuration and starts ash.
// This is to make sure the font configuration happens during ash
// initialization process.
class FontTestHelper : public test::AshTestBase {
 public:
  enum DisplayType {
    INTERNAL,
    EXTERNAL
  };

  FontTestHelper(float scale, DisplayType display_type) {
    gfx::ClearFontRenderParamsCacheForTest();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (display_type == INTERNAL)
      command_line->AppendSwitch(switches::kAshUseFirstDisplayAsInternal);
    command_line->AppendSwitchASCII(switches::kAshHostWindowBounds,
                                    StringPrintf("1000x800*%f", scale));
    SetUp();
  }

  virtual ~FontTestHelper() {
    TearDown();
  }

  // test::AshTestBase:
  virtual void TestBody() OVERRIDE {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FontTestHelper);
};


bool IsTextSubpixelPositioningEnabled() {
  gfx::FontRenderParams params =
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(false), NULL);
  return params.subpixel_positioning;
}

}  // namespace

typedef testing::Test DisplayManagerFontTest;

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf100Internal) {
  FontTestHelper helper(1.0f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f, Shell::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf125Internal) {
  FontTestHelper helper(1.25f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      1.25f, Shell::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf200Internal) {
  FontTestHelper helper(2.0f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      2.0f, Shell::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf100External) {
  FontTestHelper helper(1.0f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f, Shell::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf125External) {
  FontTestHelper helper(1.25f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      1.25f, Shell::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf200External) {
  FontTestHelper helper(2.0f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      2.0f, Shell::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
}

TEST_F(DisplayManagerFontTest,
       TextSubpixelPositioningWithDsf125InternalWithScaling) {
  DisplayInfo::SetUse125DSFForUIScaling(true);
  FontTestHelper helper(1.25f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f, Shell::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  DisplayInfo::SetUse125DSFForUIScaling(false);
}

#endif  // OS_CHROMEOS

}  // namespace ash
