// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_GESTURE_HANDLER_CLASSIC_H_
#define ASH_WM_IMMERSIVE_GESTURE_HANDLER_CLASSIC_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/immersive/immersive_gesture_handler.h"
#include "ui/events/event_handler.h"

namespace ash {

class ImmersiveFullscreenController;
class TabletModeAppWindowDragController;

// ImmersiveGestureHandler is responsible for calling
// ImmersiveFullscreenController::OnGestureEvent() to show/hide the title bar or
// TabletModeWindowDragController::DragWindowFromTop() to drag the window from
// the top if CanDrag is true when a gesture is received.
class ASH_EXPORT ImmersiveGestureHandlerClassic
    : public ImmersiveGestureHandler,
      public ui::EventHandler {
 public:
  explicit ImmersiveGestureHandlerClassic(
      ImmersiveFullscreenController* controller);
  ~ImmersiveGestureHandlerClassic() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Returns true if the target of |event| can be dragged.
  bool CanDrag(ui::GestureEvent* event);

  ImmersiveFullscreenController*
      immersive_fullscreen_controller_;  // Not owned.

  std::unique_ptr<TabletModeAppWindowDragController>
      tablet_mode_app_window_drag_controller_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveGestureHandlerClassic);
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_GESTURE_HANDLER_CLASSIC_H_
