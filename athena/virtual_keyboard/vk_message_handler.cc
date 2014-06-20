// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/virtual_keyboard/vk_message_handler.h"

#include "athena/screen/public/screen_manager.h"
#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace athena {

VKMessageHandler::VKMessageHandler() {
}
VKMessageHandler::~VKMessageHandler() {
}

void VKMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "sendKeyEvent",
      base::Bind(&VKMessageHandler::SendKeyEvent, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "hideKeyboard",
      base::Bind(&VKMessageHandler::HideKeyboard, base::Unretained(this)));
}

void VKMessageHandler::SendKeyEvent(const base::ListValue* params) {
  std::string type;
  int char_value;
  int key_code;
  std::string key_name;
  int modifiers;
  if (!params->GetString(0, &type))
    return;
  if (!params->GetInteger(1, &char_value))
    return;
  if (!params->GetInteger(2, &key_code))
    return;
  if (!params->GetString(3, &key_name))
    return;
  if (!params->GetInteger(4, &modifiers))
    return;

  aura::Window* window = ScreenManager::Get()->GetContext();
  keyboard::SendKeyEvent(
      type, char_value, key_code, key_name, modifiers, window->GetHost());
}

void VKMessageHandler::HideKeyboard(const base::ListValue* params) {
  keyboard::KeyboardController::GetInstance()->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_MANUAL);
}

}  // namespace athena
