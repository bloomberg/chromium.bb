// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_
#define MASH_WM_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_

#include "base/macros.h"

namespace gfx {
class Point;
}

namespace views {
class NonClientFrameView;
class Widget;
}

namespace mash {
namespace wm {
class FrameCaptionButtonContainerView;

// Class which manages the hittest override bounds for |frame|.
class FrameBorderHitTestController {
 public:
  // Does the non client hit test on behalf of |view|. |point_in_widget| must be
  // in the coordinates of |view|'s widget.
  static int NonClientHitTest(
      views::NonClientFrameView* view,
      FrameCaptionButtonContainerView* caption_button_container,
      const gfx::Point& point_in_widget);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FrameBorderHitTestController);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_
