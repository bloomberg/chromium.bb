// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_GAMEPAD_CONTROLLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_GAMEPAD_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace blink {
class WebFrame;
}

namespace WebTestRunner {
class WebTestDelegate;
}

namespace content {

class GamepadController : public base::SupportsWeakPtr<GamepadController> {
 public:
  GamepadController();
  ~GamepadController();

  void Reset();
  void Install(blink::WebFrame* frame);
  void SetDelegate(WebTestRunner::WebTestDelegate* delegate);

 private:
  friend class GamepadControllerBindings;

  void Connect(int index);
  void Disconnect(int index);
  void SetId(int index, const std::string& src);
  void SetButtonCount(int index, int buttons);
  void SetButtonData(int index, int button, double data);
  void SetAxisCount(int index, int axes);
  void SetAxisData(int index, int axis, double data);

  blink::WebGamepads gamepads_;

  WebTestRunner::WebTestDelegate* delegate_;

  base::WeakPtrFactory<GamepadController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GamepadController);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_GAMEPAD_CONTROLLER_H_
