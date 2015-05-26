// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_controller_common.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/test_popup_controller_common.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

class PopupControllerBaseTest : public ChromeRenderViewHostTestHarness {
 public:
  PopupControllerBaseTest() {}
  ~PopupControllerBaseTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupControllerBaseTest);
};

TEST_F(PopupControllerBaseTest, GetPopupBoundsTest) {
  int desired_width = 40;
  int desired_height = 16;

  // Set up the visible screen space.
  gfx::Display display(0,
                       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height));

  struct {
    gfx::RectF element_bounds;
    gfx::Rect expected_popup_bounds_ltr;
    // Non-empty only when it differs from the ltr expectation.
    gfx::Rect expected_popup_bounds_rtl;
  } test_cases[] = {
      // The popup grows down and to the end.
      {gfx::RectF(38, 0, 5, 0),
       gfx::Rect(38, 0, desired_width, desired_height),
       gfx::Rect(3, 0, desired_width, desired_height)},

      // The popup grows down and to the left when there's no room on the right.
      {gfx::RectF(2 * desired_width, 0, 5, 0),
       gfx::Rect(desired_width, 0, desired_width, desired_height)},

      // The popup grows up and to the right.
      {gfx::RectF(0, 2 * desired_height, 5, 0),
       gfx::Rect(0, desired_height, desired_width, desired_height)},

      // The popup grows up and to the left.
      {gfx::RectF(2 * desired_width, 2 * desired_height, 5, 0),
       gfx::Rect(desired_width, desired_height, desired_width, desired_height)},

      // The popup would be partial off the top and left side of the screen.
      {gfx::RectF(-desired_width / 2, -desired_height / 2, 5, 0),
       gfx::Rect(0, 0, desired_width, desired_height)},

      // The popup would be partially off the bottom and the right side of
      // the screen.
      {gfx::RectF(desired_width * 1.5, desired_height * 1.5, 5, 0),
       gfx::Rect((desired_width * 1.5 + 5 - desired_width),
                 (desired_height * 1.5 - desired_height), desired_width,
                 desired_height)},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    scoped_ptr<TestPopupControllerCommon> popup_controller(
        new TestPopupControllerCommon(test_cases[i].element_bounds,
                                      base::i18n::LEFT_TO_RIGHT));
    popup_controller->set_display(display);
    gfx::Rect actual_popup_bounds =
        popup_controller->GetPopupBounds(desired_width, desired_height);
    EXPECT_EQ(test_cases[i].expected_popup_bounds_ltr.ToString(),
              actual_popup_bounds.ToString())
        << "Popup bounds failed to match for ltr test " << i;

    popup_controller.reset(new TestPopupControllerCommon(
        test_cases[i].element_bounds, base::i18n::RIGHT_TO_LEFT));
    popup_controller->set_display(display);
    actual_popup_bounds =
        popup_controller->GetPopupBounds(desired_width, desired_height);
    gfx::Rect expected_popup_bounds = test_cases[i].expected_popup_bounds_rtl;
    if (expected_popup_bounds.IsEmpty())
      expected_popup_bounds = test_cases[i].expected_popup_bounds_ltr;
    EXPECT_EQ(expected_popup_bounds.ToString(), actual_popup_bounds.ToString())
        << "Popup bounds failed to match for rtl test " << i;
  }
}

}  // namespace autofill
