// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_
#define ASH_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace views {
class NonClientFrameView;
class Widget;
}

namespace ash {
class FrameCaptionButtonContainerView;

// Class which manages the hittest override bounds for |frame|.
class ASH_EXPORT FrameBorderHitTestController {
 public:
  explicit FrameBorderHitTestController(views::Widget* frame);
  virtual ~FrameBorderHitTestController();

  // Does the non client hit test on behalf of |view|. |point_in_widget| must be
  // in the coordinates of |view|'s widget.
  static int NonClientHitTest(
      views::NonClientFrameView* view,
      FrameCaptionButtonContainerView* caption_button_container,
      const gfx::Point& point_in_widget);

 private:
  // The window whose hittest override bounds are being managed.
  aura::Window* frame_window_;

  DISALLOW_COPY_AND_ASSIGN(FrameBorderHitTestController);
};

}  // namespace ash

#endif  // ASH_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_
