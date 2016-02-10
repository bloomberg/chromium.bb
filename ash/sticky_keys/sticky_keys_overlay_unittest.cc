// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sticky_keys/sticky_keys_overlay.h"

#include "ash/display/display_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace ash {

using StickyKeysOverlayTest = test::AshTestBase;

TEST_F(StickyKeysOverlayTest, OverlayVisibility) {
  StickyKeysOverlay overlay;
  EXPECT_FALSE(overlay.is_visible());
  overlay.Show(true);
  EXPECT_TRUE(overlay.is_visible());
}

TEST_F(StickyKeysOverlayTest, ModifierKeyState) {
  StickyKeysOverlay overlay;
  overlay.SetModifierKeyState(ui::EF_SHIFT_DOWN, STICKY_KEY_STATE_DISABLED);
  overlay.SetModifierKeyState(ui::EF_ALT_DOWN, STICKY_KEY_STATE_LOCKED);
  overlay.SetModifierKeyState(ui::EF_CONTROL_DOWN, STICKY_KEY_STATE_ENABLED);
  overlay.SetModifierKeyState(ui::EF_COMMAND_DOWN, STICKY_KEY_STATE_LOCKED);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay.GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay.GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay.GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay.GetModifierKeyState(ui::EF_COMMAND_DOWN));
}

// This test addresses the crash report at crbug.com/435600, speculated to be
// caused by using sticky keys with multiple displays.
TEST_F(StickyKeysOverlayTest, OverlayNotDestroyedAfterDisplayRemoved) {
  // Add a secondary display to the left of the primary one.
  UpdateDisplay("1280x1024,1980x1080");
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayIdList display_ids = display_manager->GetCurrentDisplayIdList();
  int64_t primary_display_id = display_ids[0];
  int64_t secondary_display_id = display_ids[1];
  display_manager->SetLayoutForCurrentDisplays(
      DisplayLayout(DisplayPlacement::LEFT, 0));

  // The overlay should belong to the secondary root window.
  StickyKeysOverlay overlay;
  views::Widget* overlay_widget = overlay.GetWidgetForTesting();
  WindowTreeHostManager* window_tree_host_manager =
      Shell::GetInstance()->window_tree_host_manager();
  EXPECT_EQ(
      window_tree_host_manager->GetRootWindowForDisplayId(secondary_display_id),
      overlay_widget->GetNativeWindow()->GetRootWindow());

  // Removing the second display should move the overlay to the primary root
  // window.
  UpdateDisplay("1280x1024");
  EXPECT_EQ(
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display_id),
      overlay_widget->GetNativeWindow()->GetRootWindow());

  overlay.SetModifierKeyState(ui::EF_SHIFT_DOWN, STICKY_KEY_STATE_ENABLED);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay.GetModifierKeyState(ui::EF_SHIFT_DOWN));
  overlay.SetModifierKeyState(ui::EF_SHIFT_DOWN, STICKY_KEY_STATE_DISABLED);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay.GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

// Additional sticky key overlay tests that depend on chromeos::EventRewriter
// are now in chrome/browser/chromeos/events/event_rewriter_unittest.cc .

}  // namespace ash
