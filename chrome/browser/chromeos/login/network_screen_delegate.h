// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_DELEGATE_H_
#pragma once

#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/controls/button/button.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace chromeos {

class KeyboardSwitchMenu;
class LanguageSwitchMenu;

// Interface that NetworkScreen exposes to the NetworkSelectionView.
class NetworkScreenDelegate : public views::ButtonListener,
                              public NetworkLibrary::NetworkManagerObserver {
 public:
  // Cleares all error notifications.
  virtual void ClearErrors() = 0;

  // True is MessageBubble with error message is shown.
  virtual bool is_error_shown() = 0;

  virtual LanguageSwitchMenu* language_switch_menu() = 0;
  virtual KeyboardSwitchMenu* keyboard_switch_menu() = 0;

  virtual gfx::Size size() const = 0;

 protected:
  virtual ~NetworkScreenDelegate() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_DELEGATE_H_
