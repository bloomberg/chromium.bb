// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_

#include <string>

#include "base/string16.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"

namespace chromeos {

class NetworkConfigView;

// A dialog box for showing a password textfield.
class WifiConfigView : public views::View,
                       public views::Textfield::Controller,
                       public views::ButtonListener {
 public:
  WifiConfigView(NetworkConfigView* parent, WifiNetwork wifi);
  explicit WifiConfigView(NetworkConfigView* parent);
  virtual ~WifiConfigView() {}

  // views::Textfield::Controller methods.
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents);
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke) {
    return false;
  }

  // views::ButtonListener
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Get the typed in ssid.
  const string16& GetSSID() const;
  // Get the typed in passphrase.
  const string16& GetPassphrase() const;

  // Returns true if the textfields are non-empty and we can login.
  bool can_login() const { return can_login_; }

  // Focus the first field in the UI.
  void FocusFirstField();

 private:
  // Initializes UI.
  void Init();

  NetworkConfigView* parent_;

  bool other_network_;

  // Whether or not we can log in. This gets recalculated when textfield
  // contents change.
  bool can_login_;

  WifiNetwork wifi_;

  views::Textfield* ssid_textfield_;
  views::Textfield* passphrase_textfield_;

  DISALLOW_COPY_AND_ASSIGN(WifiConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
