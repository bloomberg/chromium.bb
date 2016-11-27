// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/screens/network_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/gfx/geometry/point.h"

namespace chromeos {

class CoreOobeActor;

// WebUI implementation of NetworkScreenActor. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class NetworkScreenHandler : public NetworkView, public BaseScreenHandler {
 public:
  explicit NetworkScreenHandler(CoreOobeActor* core_oobe_actor);
  ~NetworkScreenHandler() override;

 private:
  // NetworkView implementation:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void Bind(NetworkModel& model) override;
  void Unbind() override;
  void ShowError(const base::string16& message) override;
  void ClearErrors() override;
  void StopDemoModeDetection() override;
  void ShowConnectingStatus(bool connecting,
                            const base::string16& network_id) override;
  void ReloadLocalizedContent() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

 private:
  // Returns available timezones. Caller gets the ownership.
  static base::ListValue* GetTimezoneList();

  CoreOobeActor* core_oobe_actor_;
  NetworkModel* model_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Position of the network control.
  gfx::Point network_control_pos_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
