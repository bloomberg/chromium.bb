// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard_overlay/keyboard_overlay_delegate.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

typedef test::AshTestBase KeyboardOverlayDelegateTest;

// Verifies we can show and close the widget for the overlay dialog.
TEST_F(KeyboardOverlayDelegateTest, ShowAndClose) {
  UpdateDisplay("500x400,300x200");
  KeyboardOverlayDelegate delegate(ASCIIToUTF16("Title"),
                                   GURL("chrome://keyboardoverlay/"));
  // Showing the dialog creates a widget.
  views::Widget* widget = delegate.Show(NULL);
  EXPECT_TRUE(widget);

  // The widget is on the primary root window.
  EXPECT_EQ(Shell::GetPrimaryRootWindow(),
            widget->GetNativeWindow()->GetRootWindow());

  // The widget is horizontally centered at the bottom of the work area.
  gfx::Rect work_area = Shell::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect bounds = widget->GetRestoredBounds();
  EXPECT_EQ(work_area.CenterPoint().x(), bounds.CenterPoint().x());
  EXPECT_EQ(work_area.bottom(), bounds.bottom());

  // Clean up.
  widget->CloseNow();
}

}  // namespace ash
