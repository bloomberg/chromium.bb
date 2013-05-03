// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pong_input.h"

#include <stdlib.h>

namespace {

// Input event key codes.  PPAPI uses Windows Virtual key codes.
const uint32_t kUpArrow = 0x26;
const uint32_t kDownArrow = 0x28;

/** When the distance between the ball and the center of the paddle is within
 * kPaddleAIDeadZone pixels, don't move the paddle. This keeps the paddle from
 * bouncing if it can move faster than the ball.
 */
const int kPaddleAIDeadZone = 20;

}

PongInputKeyboard::PongInputKeyboard(PongInputKeyboardDelegate* delegate)
    : delegate_(delegate) {}

MoveDirection PongInputKeyboard::GetMove(const PongModel& model,
                                         bool is_left_paddle) {
  if (delegate_->IsKeyDown(kUpArrow))
    return MOVE_UP;
  else if (delegate_->IsKeyDown(kDownArrow))
    return MOVE_DOWN;
  else
    return MOVE_NONE;
}

MoveDirection PongInputAI::GetMove(const PongModel& model,
                                   bool is_left_paddle) {
  // A highly advanced AI algorithm that moves the paddle toward the y position
  // of the ball.
  const PaddleModel& paddle =
      is_left_paddle ? model.left_paddle() : model.right_paddle();
  int ball_center_y = model.ball().rect.CenterPoint().y();
  int paddle_center_y = paddle.rect.CenterPoint().y();
  int distance_y = labs(paddle_center_y - ball_center_y);

  if (distance_y < kPaddleAIDeadZone)
    return MOVE_NONE;
  else if (paddle_center_y > ball_center_y)
    return MOVE_UP;
  else
    return MOVE_DOWN;
}
