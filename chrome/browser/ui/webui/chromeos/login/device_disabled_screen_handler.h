// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// WebUI implementation of DeviceDisabledScreenActor.
class DeviceDisabledScreenHandler : public DeviceDisabledScreenActor,
                                    public BaseScreenHandler {
 public:
  DeviceDisabledScreenHandler();
  ~DeviceDisabledScreenHandler() override;

  // DeviceDisabledScreenActor:
  void Show(const std::string& message) override;
  void Hide() override;
  void SetDelegate(Delegate* delegate) override;
  void UpdateMessage(const std::string& message) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;

  Delegate* delegate_;

  // Indicates whether the screen should be shown right after initialization.
  bool show_on_init_;

  // The message to show to the user.
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisabledScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEVICE_DISABLED_SCREEN_HANDLER_H_

