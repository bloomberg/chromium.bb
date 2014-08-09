// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_GAMEPAD_CONTROLLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_GAMEPAD_CONTROLLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace blink {
class WebFrame;
class WebGamepadListener;
}

namespace content {

class WebTestDelegate;

class GamepadController
    : public base::SupportsWeakPtr<GamepadController>,
      public RendererGamepadProvider {
 public:
  GamepadController();
  virtual ~GamepadController();

  void Reset();
  void Install(blink::WebFrame* frame);
  void SetDelegate(WebTestDelegate* delegate);

  // RendererGamepadProvider implementation.
  virtual void SampleGamepads(
      blink::WebGamepads& gamepads) OVERRIDE;
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void SendStartMessage() OVERRIDE;
  virtual void SendStopMessage() OVERRIDE;

 private:
  friend class GamepadControllerBindings;

  // TODO(b.kelemen): for historical reasons Connect just initializes the
  // object. The 'gamepadconnected' event will be dispatched via
  // DispatchConnected. Tests for connected events need to first connect(),
  // then set the gamepad data and finally call dispatchConnected().
  // We should consider renaming Connect to Init and DispatchConnected to
  // Connect and at the same time updating all the gamepad tests.
  void Connect(int index);
  void DispatchConnected(int index);

  void Disconnect(int index);
  void SetId(int index, const std::string& src);
  void SetButtonCount(int index, int buttons);
  void SetButtonData(int index, int button, double data);
  void SetAxisCount(int index, int axes);
  void SetAxisData(int index, int axis, double data);

  blink::WebGamepads gamepads_;

  // Mapping from gamepad index to connection state.
  std::map<int, bool> pending_changes_;

  base::WeakPtrFactory<GamepadController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GamepadController);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_GAMEPAD_CONTROLLER_H_
