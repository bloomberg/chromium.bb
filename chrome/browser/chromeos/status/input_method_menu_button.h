// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/status/input_method_menu.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "ui/views/controls/button/menu_button_listener.h"

namespace chromeos {

// A class for the button in the status area which expands the dropdown menu for
// switching input method and keyboard layout.
class InputMethodMenuButton : public StatusAreaButton,
                              public views::MenuButtonListener {
 public:
  explicit InputMethodMenuButton(StatusAreaButton::Delegate* delegate);
  virtual ~InputMethodMenuButton();

  // views::View implementation.
  virtual void OnLocaleChanged() OVERRIDE;

  // views::MenuButtonListener implementation.
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // InputMethodMenu implementation.
  virtual void UpdateUI(const std::string& input_method_id,
                        const string16& name,
                        const string16& tooltip,
                        size_t num_active_input_methods);
  virtual bool ShouldSupportConfigUI();
  virtual void OpenConfigUI();

  // Updates the UI from the current input method.
  void UpdateUIFromCurrentInputMethod();

 private:
  // Returns true if the Chrome window where the button lives is active. When
  // Ash is in use, the function always returns true.
  bool StatusAreaIsVisible();

  scoped_ptr<InputMethodMenu> menu_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_BUTTON_H_
