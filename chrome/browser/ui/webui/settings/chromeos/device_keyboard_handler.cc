// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"

#include "ash/new_window_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/web_ui.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/keyboard_device.h"

namespace {

bool HasExternalKeyboard() {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->keyboard_devices()) {
    if (keyboard.type == ui::InputDeviceType::INPUT_DEVICE_EXTERNAL)
      return true;
  }

  return false;
}

}  // namespace

namespace chromeos {
namespace settings {

KeyboardHandler::KeyboardHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

KeyboardHandler::~KeyboardHandler() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

void KeyboardHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeKeyboardSettings",
      base::Bind(&KeyboardHandler::HandleInitialize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showKeyboardShortcutsOverlay",
      base::Bind(&KeyboardHandler::HandleShowKeyboardShortcutsOverlay,
                 base::Unretained(this)));
}

void KeyboardHandler::OnKeyboardDeviceConfigurationChanged() {
  UpdateShowKeys();
}

void KeyboardHandler::HandleInitialize(const base::ListValue* args) {
  UpdateShowKeys();
}

void KeyboardHandler::HandleShowKeyboardShortcutsOverlay(
    const base::ListValue* args) const {
  ash::Shell::GetInstance()->new_window_delegate()->ShowKeyboardOverlay();
}

void KeyboardHandler::UpdateShowKeys() const {
  const base::FundamentalValue has_caps_lock(HasExternalKeyboard());
  const base::FundamentalValue has_diamond_key(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kHasChromeOSDiamondKey));
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback",
                                   base::StringValue("show-keys-changed"),
                                   has_caps_lock,
                                   has_diamond_key);
}

}  // namespace settings
}  // namespace chromeos
