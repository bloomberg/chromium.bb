// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/gamepad_controller.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/platform/WebGamepadListener.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "v8/include/v8.h"

using blink::WebFrame;
using blink::WebGamepad;
using blink::WebGamepads;

namespace content {

class GamepadControllerBindings
    : public gin::Wrappable<GamepadControllerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<GamepadController> controller,
                      blink::WebFrame* frame);

 private:
  explicit GamepadControllerBindings(
      base::WeakPtr<GamepadController> controller);
  virtual ~GamepadControllerBindings();

  // gin::Wrappable.
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  void Connect(int index);
  void DispatchConnected(int index);
  void Disconnect(int index);
  void SetId(int index, const std::string& src);
  void SetButtonCount(int index, int buttons);
  void SetButtonData(int index, int button, double data);
  void SetAxisCount(int index, int axes);
  void SetAxisData(int index, int axis, double data);

  base::WeakPtr<GamepadController> controller_;

  DISALLOW_COPY_AND_ASSIGN(GamepadControllerBindings);
};

gin::WrapperInfo GamepadControllerBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void GamepadControllerBindings::Install(
    base::WeakPtr<GamepadController> controller,
    WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GamepadControllerBindings> bindings =
      gin::CreateHandle(isolate, new GamepadControllerBindings(controller));
  if (bindings.IsEmpty())
    return;
  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "gamepadController"), bindings.ToV8());
}

GamepadControllerBindings::GamepadControllerBindings(
    base::WeakPtr<GamepadController> controller)
    : controller_(controller) {}

GamepadControllerBindings::~GamepadControllerBindings() {}

gin::ObjectTemplateBuilder GamepadControllerBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GamepadControllerBindings>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("connect", &GamepadControllerBindings::Connect)
      .SetMethod("dispatchConnected", &GamepadControllerBindings::DispatchConnected)
      .SetMethod("disconnect", &GamepadControllerBindings::Disconnect)
      .SetMethod("setId", &GamepadControllerBindings::SetId)
      .SetMethod("setButtonCount", &GamepadControllerBindings::SetButtonCount)
      .SetMethod("setButtonData", &GamepadControllerBindings::SetButtonData)
      .SetMethod("setAxisCount", &GamepadControllerBindings::SetAxisCount)
      .SetMethod("setAxisData", &GamepadControllerBindings::SetAxisData);
}

void GamepadControllerBindings::Connect(int index) {
  if (controller_)
    controller_->Connect(index);
}

void GamepadControllerBindings::DispatchConnected(int index) {
  if (controller_)
    controller_->DispatchConnected(index);
}

void GamepadControllerBindings::Disconnect(int index) {
  if (controller_)
    controller_->Disconnect(index);
}

void GamepadControllerBindings::SetId(int index, const std::string& src) {
  if (controller_)
    controller_->SetId(index, src);
}

void GamepadControllerBindings::SetButtonCount(int index, int buttons) {
  if (controller_)
    controller_->SetButtonCount(index, buttons);
}

void GamepadControllerBindings::SetButtonData(int index,
                                              int button,
                                              double data) {
  if (controller_)
    controller_->SetButtonData(index, button, data);
}

void GamepadControllerBindings::SetAxisCount(int index, int axes) {
  if (controller_)
    controller_->SetAxisCount(index, axes);
}

void GamepadControllerBindings::SetAxisData(int index, int axis, double data) {
  if (controller_)
    controller_->SetAxisData(index, axis, data);
}

GamepadController::GamepadController()
    : listener_(NULL),
      weak_factory_(this) {
  Reset();
}

GamepadController::~GamepadController() {}

void GamepadController::Reset() {
  memset(&gamepads_, 0, sizeof(gamepads_));
}

void GamepadController::Install(WebFrame* frame) {
  GamepadControllerBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void GamepadController::SetDelegate(WebTestDelegate* delegate) {
  if (!delegate)
    return;
  delegate->setGamepadProvider(this);
}

void GamepadController::SampleGamepads(blink::WebGamepads& gamepads) {
  memcpy(&gamepads, &gamepads_, sizeof(blink::WebGamepads));
}

void GamepadController::SetGamepadListener(
    blink::WebGamepadListener* listener) {
  listener_ = listener;
}

void GamepadController::Connect(int index) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
    return;
  gamepads_.items[index].connected = true;
  gamepads_.length = 0;
  for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (gamepads_.items[i].connected)
      gamepads_.length = i + 1;
  }
}

void GamepadController::DispatchConnected(int index) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap)
      || !gamepads_.items[index].connected)
    return;
  const WebGamepad& pad = gamepads_.items[index];
  if (listener_)
    listener_->didConnectGamepad(index, pad);
}

void GamepadController::Disconnect(int index) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
    return;
  WebGamepad& pad = gamepads_.items[index];
  pad.connected = false;
  gamepads_.length = 0;
  for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (gamepads_.items[i].connected)
      gamepads_.length = i + 1;
  }
  if (listener_)
    listener_->didDisconnectGamepad(index, pad);
}

void GamepadController::SetId(int index, const std::string& src) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
    return;
  const char* p = src.c_str();
  memset(gamepads_.items[index].id, 0, sizeof(gamepads_.items[index].id));
  for (unsigned i = 0; *p && i < WebGamepad::idLengthCap - 1; ++i)
    gamepads_.items[index].id[i] = *p++;
}

void GamepadController::SetButtonCount(int index, int buttons) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
    return;
  if (buttons < 0 || buttons >= static_cast<int>(WebGamepad::buttonsLengthCap))
    return;
  gamepads_.items[index].buttonsLength = buttons;
}

void GamepadController::SetButtonData(int index, int button, double data) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
    return;
  if (button < 0 || button >= static_cast<int>(WebGamepad::buttonsLengthCap))
    return;
  gamepads_.items[index].buttons[button].value = data;
  gamepads_.items[index].buttons[button].pressed = data > 0.1f;
}

void GamepadController::SetAxisCount(int index, int axes) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
    return;
  if (axes < 0 || axes >= static_cast<int>(WebGamepad::axesLengthCap))
    return;
  gamepads_.items[index].axesLength = axes;
}

void GamepadController::SetAxisData(int index, int axis, double data) {
  if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
    return;
  if (axis < 0 || axis >= static_cast<int>(WebGamepad::axesLengthCap))
    return;
  gamepads_.items[index].axes[axis] = data;
}

}  // namespace content
