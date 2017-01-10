// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

namespace options {
class CoreChromeOSOptionsHandler;
class BluetoothOptionsHandler;
}

// A WebUI to host bluetooth device pairing web ui.
class BluetoothPairingUI : public ui::WebDialogUI,
                           public ::options::OptionsPageUIHandlerHost {
 public:
  explicit BluetoothPairingUI(content::WebUI* web_ui);
  ~BluetoothPairingUI() override;

 private:
  // Overridden from OptionsPageUIHandlerHost:
  void InitializeHandlers() override;

  options::CoreChromeOSOptionsHandler* core_handler_ = nullptr;
  options::BluetoothOptionsHandler* bluetooth_handler_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BluetoothPairingUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_UI_H_
