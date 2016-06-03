// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_FRAME_FRAME_BORDER_HIT_TEST_CONTROLLER_H_
#define ASH_MUS_FRAME_FRAME_BORDER_HIT_TEST_CONTROLLER_H_

#include "base/macros.h"

namespace gfx {
class Insets;
class Point;
}

namespace views {
class NonClientFrameView;
class Widget;
}

namespace ash {
namespace mus {
class FrameCaptionButtonContainerView;

// Class which manages the hittest override bounds for |frame|.
class FrameBorderHitTestController {
 public:
  // Returns the amount of space resizes are allowed to occur outside the
  // bounds of windows.
  static gfx::Insets GetResizeOutsideBoundsSize();

  // Does the non client hit test on behalf of |view|. |point_in_widget| must be
  // in the coordinates of |view|'s widget.
  static int NonClientHitTest(
      views::NonClientFrameView* view,
      FrameCaptionButtonContainerView* caption_button_container,
      const gfx::Point& point_in_widget);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FrameBorderHitTestController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_FRAME_BORDER_HIT_TEST_CONTROLLER_H_
