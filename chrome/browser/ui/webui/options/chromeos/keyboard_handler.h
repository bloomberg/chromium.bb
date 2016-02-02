// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KEYBOARD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KEYBOARD_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/prefs/pref_member.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace chromeos {
namespace options {

// Customize modifier keys overlay page UI handler.
class KeyboardHandler
    : public ::options::OptionsPageUIHandler,
      public ui::InputDeviceEventObserver {
 public:
  KeyboardHandler();
  ~KeyboardHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;
  void RegisterMessages() override;

  // ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;

 private:
  // Show the keyboard shortcuts overlay from the options page.
  void HandleShowKeyboardShortcuts(const base::ListValue* args);

  // Shows or hides the CapsLock options based on whether or not there is an
  // external keyboard connected.
  void UpdateCapsLockOptions() const;

  DISALLOW_COPY_AND_ASSIGN(KeyboardHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KEYBOARD_HANDLER_H_
