// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/screens/reset_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/web_ui.h"

class PrefRegistrySimple;

namespace chromeos {

// WebUI implementation of ResetScreenActor.
class ResetScreenHandler : public ResetScreenActor,
                           public BaseScreenHandler,
                           public UpdateEngineClient::Observer {
 public:
  ResetScreenHandler();
  virtual ~ResetScreenHandler();

  // ResetScreenActor implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // UpdateEngineClient::Observer implementation:
  virtual void UpdateStatusChanged(
      const UpdateEngineClient::Status& status) OVERRIDE;

  void OnRollbackCheck(bool can_rollback);

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // JS messages handlers.
  void HandleOnCancel();
  void HandleOnRestart();
  void HandleOnPowerwash(bool rollback_checked);
  void HandleOnLearnMore();
  void HandleOnShowRollback();
  void HandleOnHideRollback();
  void HandleOnShowConfirm();

  void ChooseAndApplyShowScenario();
  void ShowWithParams();

  Delegate* delegate_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Keeps whether restart is required before reset.
  // False if first exec after boot.
  bool restart_required_;

  // Keeps whether previous reboot was requested from reset screen. Makes sense
  // for first exec after boot situation.
  bool reboot_was_requested_;

  // Keeps whether rollback option is available.
  bool rollback_available_;

  // Keeps whether rollback option is active at the screen.
  bool rollback_checked_;

  // Whether rollback is initiated. Prevents screen-hide.
  bool preparing_for_rollback_;

  base::WeakPtrFactory<ResetScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResetScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
