// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_BUTTON_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/status/input_method_menu.h"
#include "chrome/browser/chromeos/status/status_area_button.h"

namespace chromeos {

class StatusAreaHost;

// A class for the button in the status area which expands the dropdown menu for
// switching input method and keyboard layout.
class InputMethodMenuButton : public StatusAreaButton,
                              public InputMethodMenu {
 public:
  explicit InputMethodMenuButton(StatusAreaHost* host);
  virtual ~InputMethodMenuButton() {}

  // views::View implementation.
  virtual void OnLocaleChanged();

 private:
  // InputMethodMenu implementation.
  virtual void UpdateUI(
      const std::wstring& name, const std::wstring& tooltip);
  virtual bool ShouldSupportConfigUI();
  virtual void OpenConfigUI();

  StatusAreaHost* host_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_BUTTON_H_
