// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_ENABLE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_ENABLE_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// WebUI implementation of KioskEnableScreenActor.
class KioskEnableScreenHandler : public KioskEnableScreenActor,
                                 public BaseScreenHandler {
 public:
  KioskEnableScreenHandler();
  virtual ~KioskEnableScreenHandler();

  // KioskEnableScreenActor implementation:
  virtual void Show() OVERRIDE;
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // JS messages handlers.
  void HandleOnClose();
  void HandleOnEnable();

  // Callback for KioskAppManager::EnableConsumerModeKiosk().
  void OnEnableConsumerKioskAutoLaunch(bool success);

  // Callback for KioskAppManager::GetConsumerKioskModeStatus().
  void OnGetConsumerKioskAutoLaunchStatus(
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  Delegate* delegate_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // True if machine's consumer kiosk mode is in a configurable state.
  bool is_configurable_;

  base::WeakPtrFactory<KioskEnableScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(KioskEnableScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_ENABLE_SCREEN_HANDLER_H_
