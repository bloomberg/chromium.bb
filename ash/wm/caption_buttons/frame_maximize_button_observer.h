// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTION_BUTTONS_FRAME_MAXIMIZE_BUTTON_OBSERVER_H_
#define ASH_WM_CAPTION_BUTTONS_FRAME_MAXIMIZE_BUTTON_OBSERVER_H_

#include "ash/ash_export.h"

namespace views {
class Widget;
}

namespace ash {

class ASH_EXPORT FrameMaximizeButtonObserver {
 public:
  virtual ~FrameMaximizeButtonObserver() {}

  // Called when the maximize button's help bubble is shown.
  virtual void OnMaximizeBubbleShown(views::Widget* bubble) = 0;
};

}  // namespace ash

#endif  // ASH_WM_CAPTION_BUTTONS_FRAME_MAXIMIZE_BUTTON_OBSERVER_H_
