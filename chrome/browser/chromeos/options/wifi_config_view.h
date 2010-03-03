// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_

#include <string>

#include "base/string16.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"

namespace chromeos {

// A dialog box for showing a password textfield.
class WifiConfigView : public views::View {
 public:
  explicit WifiConfigView(WifiNetwork wifi);
  explicit WifiConfigView();
  virtual ~WifiConfigView() {}

  // Get the typed in ssid.
  const string16& GetSSID() const;
  // Get the typed in passphrase.
  const string16& GetPassphrase() const;

 private:
  // Initializes UI.
  void Init();

  bool other_network_;

  WifiNetwork wifi_;

  views::Textfield* ssid_textfield_;
  views::Textfield* passphrase_textfield_;

  DISALLOW_COPY_AND_ASSIGN(WifiConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
