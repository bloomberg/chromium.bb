// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KEYBOARD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KEYBOARD_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {
namespace options {

// Customize modifier keys overlay page UI handler.
class KeyboardHandler : public ::options::OptionsPageUIHandler {
 public:
  KeyboardHandler();
  virtual ~KeyboardHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Show the keyboard shortcuts overlay from the options page.
  void HandleShowKeyboardShortcuts(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(KeyboardHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KEYBOARD_HANDLER_H_
