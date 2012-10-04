// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_PONG_PONG_INPUT_H_
#define EXAMPLES_PONG_PONG_INPUT_H_

#include "pong_model.h"

class PongInput {
 public:
  virtual ~PongInput() {}
  virtual MoveDirection GetMove(const PongModel& model,
                                bool is_left_paddle) = 0;
};

class PongInputKeyboardDelegate {
 public:
  virtual bool IsKeyDown(int key_code) = 0;
};

class PongInputKeyboard : public PongInput {
 public:
  PongInputKeyboard(PongInputKeyboardDelegate* delegate);
  virtual MoveDirection GetMove(const PongModel& model, bool is_left_paddle);

 private:
  PongInputKeyboardDelegate* delegate_;  // Weak pointer.
};

class PongInputAI : public PongInput {
 public:
  virtual MoveDirection GetMove(const PongModel& model, bool is_left_paddle);
};

#endif  // EXAMPLES_PONG_PONG_INPUT_H_
