// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard_overlay/keyboard_overlay_delegate.h"

#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

class KeyboardOverlayDelegateTest
    : public test::AshTestBase,
      public testing::WithParamInterface<ShelfAlignment> {
 public:
  KeyboardOverlayDelegateTest() : shelf_alignment_(GetParam()) {}
  virtual ~KeyboardOverlayDelegateTest() {}
  ShelfAlignment shelf_alignment() const { return shelf_alignment_; }

 private:
  ShelfAlignment shelf_alignment_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayDelegateTest);
};

// Verifies we can show and close the widget for the overlay dialog.
TEST_P(KeyboardOverlayDelegateTest, ShowAndClose) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x400,300x200");
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(shelf_alignment(), shell->GetPrimaryRootWindow());
  KeyboardOverlayDelegate delegate(base::ASCIIToUTF16("Title"),
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

// Tests run three times - for all possible values of shelf alignment
INSTANTIATE_TEST_CASE_P(ShelfAlignmentAny,
                        KeyboardOverlayDelegateTest,
                        testing::Values(SHELF_ALIGNMENT_BOTTOM,
                                        SHELF_ALIGNMENT_LEFT,
                                        SHELF_ALIGNMENT_RIGHT,
                                        SHELF_ALIGNMENT_TOP));

}  // namespace ash
