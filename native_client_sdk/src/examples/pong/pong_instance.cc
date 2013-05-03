// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <string>
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"

#include "pong_instance.h"
#include "pong_view.h"

namespace {

const uint32_t kUpArrow = 0x26;
const uint32_t kDownArrow = 0x28;

const int32_t kMaxPointsAllowed = 256;
const int32_t kUpdateInterval = 17;  // milliseconds

const char kResetScoreMethodId[] = "resetScore";

}  // namespace

PongInstance::PongInstance(PP_Instance instance)
    : pp::Instance(instance),
      factory_(this),
      model_(NULL),
      view_(NULL),
      is_initial_view_change_(true) {
  // Request to receive input events.
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_KEYBOARD);
}

PongInstance::~PongInstance() {
  delete right_paddle_input_;
  delete left_paddle_input_;
  delete view_;
  delete model_;
}

bool PongInstance::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  model_ = new PongModel(this);
  view_ = new PongView(model_);
  left_paddle_input_ = new PongInputKeyboard(this);
  right_paddle_input_ = new PongInputAI();

  UpdateScoreDisplay();
  return true;
}

void PongInstance::DidChangeView(const pp::View& view) {
  if (!view_->DidChangeView(this, view, is_initial_view_change_)) {
    PostMessage(
        pp::Var("ERROR DidChangeView failed. Could not bind graphics?"));
    return;
  }

  model_->SetCourtSize(view_->GetSize());
  model_->ResetPositions();

  if (is_initial_view_change_) {
    ScheduleUpdate();
    is_initial_view_change_ = false;
  }
}

bool PongInstance::HandleInputEvent(const pp::InputEvent& event) {
  if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEUP ||
      event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN) {
    // By notifying the browser mouse clicks are handled, the application window
    // is able to get focus and receive key events.
    return true;
  } else if (event.GetType() == PP_INPUTEVENT_TYPE_KEYUP) {
    pp::KeyboardInputEvent key = pp::KeyboardInputEvent(event);
    key_map_[key.GetKeyCode()] = false;
    return true;
  } else if (event.GetType() == PP_INPUTEVENT_TYPE_KEYDOWN) {
    pp::KeyboardInputEvent key = pp::KeyboardInputEvent(event);
    key_map_[key.GetKeyCode()] = true;
    return true;
  }
  return false;
}

void PongInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string())
    return;
  std::string message = var_message.AsString();
  if (message == kResetScoreMethodId) {
    ResetScore();
  }
}

void PongInstance::OnScoreChanged() {
  if (model_->left_score() > kMaxPointsAllowed ||
      model_->right_score() > kMaxPointsAllowed) {
    ResetScore();
    return;
  }

  UpdateScoreDisplay();
}

void PongInstance::OnPlayerScored() { model_->ResetPositions(); }

bool PongInstance::IsKeyDown(int key_code) { return key_map_[key_code]; }

void PongInstance::ScheduleUpdate() {
  pp::Module::Get()->core()->CallOnMainThread(
      kUpdateInterval, factory_.NewCallback(&PongInstance::UpdateCallback));
}

void PongInstance::UpdateCallback(int32_t result) {
  // This is the game loop; UpdateCallback schedules another call to itself to
  // occur kUpdateInterval milliseconds later.
  ScheduleUpdate();

  MoveDirection left_move = left_paddle_input_->GetMove(*model_, true);
  MoveDirection right_move = right_paddle_input_->GetMove(*model_, false);
  model_->Update(left_move, right_move);
}

void PongInstance::ResetScore() { model_->SetScore(0, 0); }

void PongInstance::UpdateScoreDisplay() {
  char buffer[100];
  snprintf(&buffer[0],
           sizeof(buffer),
           "You %d:  Computer %d",
           model_->left_score(),
           model_->right_score());
  PostMessage(pp::Var(buffer));
}
