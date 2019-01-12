// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/screens/network_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class CoreOobeView;

// WebUI implementation of NetworkScreenView. It is used to interact with
// the OOBE network selection screen.
class NetworkScreenHandler : public NetworkScreenView,
                             public BaseScreenHandler {
 public:
  explicit NetworkScreenHandler(CoreOobeView* core_oobe_view);
  ~NetworkScreenHandler() override;

 private:
  // NetworkScreenView:
  void Show() override;
  void Hide() override;
  void Bind(NetworkScreen* screen) override;
  void Unbind() override;
  void ShowError(const base::string16& message) override;
  void ClearErrors() override;
  void ShowConnectingStatus(bool connecting,
                            const base::string16& network_id) override;
  void SetOfflineDemoModeEnabled(bool enabled) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

  CoreOobeView* core_oobe_view_ = nullptr;
  NetworkScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
