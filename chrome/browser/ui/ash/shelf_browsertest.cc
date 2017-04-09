// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/wm_shelf.h"
#include "ash/wm_window.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user_names.h"

using ShelfBrowserTest = InProcessBrowserTest;

// Confirm that a status bubble doesn't cause the shelf to darken.
IN_PROC_BROWSER_TEST_F(ShelfBrowserTest, StatusBubble) {
  ash::ShelfLayoutManager* shelf_layout_manager =
      ash::WmShelf::ForWindow(
          ash::WmWindow::Get(browser()->window()->GetNativeWindow()))
          ->shelf_layout_manager();
  EXPECT_TRUE(shelf_layout_manager->IsVisible());

  // Ensure that the browser abuts the shelf.
  gfx::Rect bounds = browser()->window()->GetBounds();
  bounds.set_height(shelf_layout_manager->GetIdealBounds().y() - bounds.y());
  browser()->window()->SetBounds(bounds);
  EXPECT_FALSE(shelf_layout_manager->window_overlaps_shelf());

  // Show status, which may overlap the shelf by a pixel.
  browser()->window()->GetStatusBubble()->SetStatus(
      base::UTF8ToUTF16("Dummy Status Text"));
  shelf_layout_manager->UpdateVisibilityState();

  // Ensure that status doesn't cause overlap.
  EXPECT_FALSE(shelf_layout_manager->window_overlaps_shelf());

  // Ensure that moving the browser slightly down does cause overlap.
  bounds.Offset(0, 1);
  browser()->window()->SetBounds(bounds);
  EXPECT_TRUE(shelf_layout_manager->window_overlaps_shelf());
}

class ShelfGuestSessionBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }
};

// Tests that in guest session, shelf alignment could be initialized to bottom
// aligned, instead of bottom locked (crbug.com/699661).
IN_PROC_BROWSER_TEST_F(ShelfGuestSessionBrowserTest, ShelfAlignment) {
  ash::WmShelf* shelf = ash::WmShelf::ForWindow(
      ash::WmWindow::Get(browser()->window()->GetNativeWindow()));
  EXPECT_EQ(ash::SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
}
