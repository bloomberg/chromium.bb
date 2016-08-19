// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_GESTURE_HANDLER_AURA_H_
#define ASH_WM_IMMERSIVE_GESTURE_HANDLER_AURA_H_

#include "ash/ash_export.h"
#include "ash/shared/immersive_gesture_handler.h"
#include "ui/events/event_handler.h"

namespace ash {

class ImmersiveFullscreenController;

// ImmersiveGestureHandler is responsible for calling
// ImmersiveFullscreenController::OnGestureEvent() when a gesture is received.
class ASH_EXPORT ImmersiveGestureHandlerAura : public ImmersiveGestureHandler,
                                               public ui::EventHandler {
 public:
  explicit ImmersiveGestureHandlerAura(
      ImmersiveFullscreenController* controller);
  ~ImmersiveGestureHandlerAura() override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  ImmersiveFullscreenController* immersive_fullscreen_controller_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveGestureHandlerAura);
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_GESTURE_HANDLER_AURA_H_
