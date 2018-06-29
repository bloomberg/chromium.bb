// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_GESTURE_HANDLER_H_
#define ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_GESTURE_HANDLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// ImmersiveGestureHandler is responsible for calling
// ImmersiveFullscreenController::OnGestureEvent() to show/hide the title bar or
// TabletModeWindowDragController::DragWindowFromTop() to drag the window from
// the top if CanDragWindow is true when a gesture is received.
class ASH_PUBLIC_EXPORT ImmersiveGestureHandler {
 public:
  virtual ~ImmersiveGestureHandler() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_GESTURE_HANDLER_H_
