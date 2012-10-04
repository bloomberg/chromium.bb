// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_PONG_PONG_H_
#define EXAMPLES_PONG_PONG_H_

#include <map>
#include "ppapi/cpp/instance.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "pong_input.h"
#include "pong_model.h"

class PongView;

class PongInstance : public pp::Instance,
                     public PongModelDelegate,
                     public PongInputKeyboardDelegate {
 public:
  explicit PongInstance(PP_Instance instance);
  virtual ~PongInstance();

  /** Called when the module is initialized. */
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  /** Called when the module's size changes, or when its visibility changes. */
  virtual void DidChangeView(const pp::View& view);

  /** Called when the module is given mouse or keyboard input. */
  virtual bool HandleInputEvent(const pp::InputEvent& event);

  /** Called when JavaScript calls postMessage() on this embedded module.
   * The only message handled here is 'resetScore', which resets the score for
   * both players to 0.
   */
  virtual void HandleMessage(const pp::Var& var_message);

  // PongModelDelegate implementation.
  virtual void OnScoreChanged();
  virtual void OnPlayerScored();

  // PongInputKeyboardDelegate implementation.
  virtual bool IsKeyDown(int key_code);

 private:
  void ScheduleUpdate();
  void UpdateCallback(int32_t result);

  void ResetScore();
  void UpdateScoreDisplay();

  pp::CompletionCallbackFactory<PongInstance> factory_;
  PongModel* model_;
  PongView* view_;
  PongInput* left_paddle_input_;
  PongInput* right_paddle_input_;
  std::map<uint32_t, bool> key_map_;
  bool is_initial_view_change_;

  // Disallow copy constructor and assignment operator.
  PongInstance(const PongInstance&);
  PongInstance& operator=(const PongInstance&);
};

#endif  // EXAMPLES_PONG_PONG_H_
