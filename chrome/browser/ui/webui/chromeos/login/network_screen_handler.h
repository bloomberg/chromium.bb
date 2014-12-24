// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/screens/network_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/point.h"

namespace chromeos {

class CoreOobeActor;
class IdleDetector;
class InputEventsBlocker;

// WebUI implementation of NetworkScreenActor. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class NetworkScreenHandler : public NetworkView, public BaseScreenHandler {
 public:
  explicit NetworkScreenHandler(CoreOobeActor* core_oobe_actor);
  virtual ~NetworkScreenHandler();

 private:
  // NetworkView implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void Bind(NetworkModel& model) override;
  virtual void Unbind() override;
  virtual void ShowError(const base::string16& message) override;
  virtual void ClearErrors() override;
  virtual void StopDemoModeDetection() override;
  virtual void ShowConnectingStatus(bool connecting,
                                    const base::string16& network_id) override;
  virtual void ReloadLocalizedContent() override;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  virtual void GetAdditionalParameters(base::DictionaryValue* dict) override;
  virtual void Initialize() override;

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
