// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/workspace_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/test/base/in_process_browser_test.h"

typedef InProcessBrowserTest ShelfBrowserTest;

// Confirm that a status bubble doesn't cause the shelf to darken.
IN_PROC_BROWSER_TEST_F(ShelfBrowserTest, StatusBubble) {
  ash::ShelfLayoutManager* shelf =
      ash::RootWindowController::ForShelf(
          browser()->window()->GetNativeWindow())->GetShelfLayoutManager();
  EXPECT_TRUE(shelf->IsVisible());

  // Ensure that the browser abuts the shelf.
  const gfx::Rect old_bounds = browser()->window()->GetBounds();
  const gfx::Rect new_bounds(
      old_bounds.x(),
      old_bounds.y(),
      old_bounds.width(),
      shelf->GetIdealBounds().y() - old_bounds.y());
  browser()->window()->SetBounds(new_bounds);
  EXPECT_FALSE(shelf->window_overlaps_shelf());

  // Show status, which will overlap the shelf by a pixel.
  browser()->window()->GetStatusBubble()->SetStatus(
      base::UTF8ToUTF16("Dummy Status Text"));
  shelf->UpdateVisibilityState();

  // Ensure that status doesn't cause overlap.
  EXPECT_FALSE(shelf->window_overlaps_shelf());
}
