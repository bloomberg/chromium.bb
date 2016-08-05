// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_
#define ASH_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

// Class which manages the hittest override bounds for |frame|.
// TODO(sky): remove this class entirely. It isn't really needed.
class ASH_EXPORT FrameBorderHitTestController {
 public:
  explicit FrameBorderHitTestController(views::Widget* frame);
  virtual ~FrameBorderHitTestController();

 private:
  // The window whose hittest override bounds are being managed.
  aura::Window* frame_window_;

  DISALLOW_COPY_AND_ASSIGN(FrameBorderHitTestController);
};

}  // namespace ash

#endif  // ASH_FRAME_FRAME_BORDER_HITTEST_CONTROLLER_H_
