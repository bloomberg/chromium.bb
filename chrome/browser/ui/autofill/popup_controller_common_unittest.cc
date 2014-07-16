// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_controller_common.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/test_popup_controller_common.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace autofill {

class PopupControllerBaseTest : public ChromeRenderViewHostTestHarness {
 public:
  PopupControllerBaseTest() {}
  virtual ~PopupControllerBaseTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupControllerBaseTest);
};

TEST_F(PopupControllerBaseTest, GetPopupBoundsTest) {
  int desired_width = 40;
  int desired_height = 16;

  // Set up the visible screen space.
  gfx::Display display(0,
                       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height));

  // Store the possible element bounds and the popup bounds they should result
  // in.
  std::vector<gfx::RectF> element_bounds;
  std::vector<gfx::Rect> expected_popup_bounds;

  // The popup grows down and to the right.
  element_bounds.push_back(gfx::RectF(0, 0, 0, 0));
  expected_popup_bounds.push_back(
      gfx::Rect(0, 0, desired_width, desired_height));

  // The popup grows down and to the left.
  element_bounds.push_back(gfx::RectF(2 * desired_width, 0, 0, 0));
  expected_popup_bounds.push_back(
      gfx::Rect(desired_width, 0, desired_width, desired_height));

  // The popup grows up and to the right.
  element_bounds.push_back(gfx::RectF(0, 2 * desired_height, 0, 0));
  expected_popup_bounds.push_back(
      gfx::Rect(0, desired_height, desired_width, desired_height));

  // The popup grows up and to the left.
  element_bounds.push_back(
      gfx::RectF(2 * desired_width, 2 * desired_height, 0, 0));
  expected_popup_bounds.push_back(
      gfx::Rect(desired_width, desired_height, desired_width, desired_height));

  // The popup would be partial off the top and left side of the screen.
  element_bounds.push_back(
      gfx::RectF(-desired_width / 2, -desired_height / 2, 0, 0));
  expected_popup_bounds.push_back(
      gfx::Rect(0, 0, desired_width, desired_height));

  // The popup would be partially off the bottom and the right side of
  // the screen.
  element_bounds.push_back(
      gfx::RectF(desired_width * 1.5, desired_height * 1.5, 0, 0));
  expected_popup_bounds.push_back(
      gfx::Rect((desired_width + 1) / 2, (desired_height + 1) / 2,
                desired_width, desired_height));

  for (size_t i = 0; i < element_bounds.size(); ++i) {
    scoped_ptr<TestPopupControllerCommon> popup_controller(
        new TestPopupControllerCommon(element_bounds[i]));
    popup_controller->set_display(display);
    gfx::Rect actual_popup_bounds =
        popup_controller->GetPopupBounds(desired_width, desired_height);

    EXPECT_EQ(expected_popup_bounds[i].ToString(),
              actual_popup_bounds.ToString()) <<
        "Popup bounds failed to match for test " << i;
  }
}

}  // namespace autofill
