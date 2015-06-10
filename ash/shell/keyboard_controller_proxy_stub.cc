// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/keyboard_controller_proxy_stub.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/mock_input_method.h"

using namespace content;

namespace ash {

KeyboardControllerProxyStub::KeyboardControllerProxyStub()
    : keyboard::KeyboardControllerProxy(Shell::GetInstance()
                                            ->delegate()
                                            ->GetActiveBrowserContext()) {
}

KeyboardControllerProxyStub::~KeyboardControllerProxyStub() {
}

bool KeyboardControllerProxyStub::HasKeyboardWindow() const {
  return keyboard_;
}

aura::Window* KeyboardControllerProxyStub::GetKeyboardWindow() {
  if (!keyboard_) {
    keyboard_.reset(new aura::Window(&delegate_));
    keyboard_->Init(ui::LAYER_NOT_DRAWN);
  }
  return keyboard_.get();
}

ui::InputMethod* KeyboardControllerProxyStub::GetInputMethod() {
  aura::Window* active_window = wm::GetActiveWindow();
  aura::Window* root_window = active_window ? active_window->GetRootWindow()
                                            : Shell::GetPrimaryRootWindow();
  return root_window->GetHost()->GetInputMethod();
}

void KeyboardControllerProxyStub::RequestAudioInput(
    WebContents* web_contents,
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
}

void KeyboardControllerProxyStub::LoadSystemKeyboard() {
}

void KeyboardControllerProxyStub::ReloadKeyboardIfNeeded() {
}

}  // namespace ash
