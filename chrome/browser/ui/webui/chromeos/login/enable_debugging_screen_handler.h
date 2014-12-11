// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/screens/enable_debugging_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

class PrefRegistrySimple;

namespace chromeos {

// WebUI implementation of EnableDebuggingScreenActor.
class EnableDebuggingScreenHandler : public EnableDebuggingScreenActor,
                                     public BaseScreenHandler {
 public:
  EnableDebuggingScreenHandler();
  virtual ~EnableDebuggingScreenHandler();

  // EnableDebuggingScreenActor implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void SetDelegate(Delegate* delegate) override;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  virtual void Initialize() override;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() override;

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  enum UIState {
    UI_STATE_ERROR = -1,
    UI_STATE_REMOVE_PROTECTION = 1,
    UI_STATE_SETUP = 2,
    UI_STATE_WAIT = 3,
    UI_STATE_DONE = 4,
  };

  // JS messages handlers.
  void HandleOnCancel();
  void HandleOnDone();
  void HandleOnLearnMore();
  void HandleOnRemoveRootFSProtection();
  void HandleOnSetup(const std::string& password);

  void ShowWithParams();

  // Callback for DebugDaemonClient::WaitForServiceToBeAvailable
  void OnServiceAvailabilityChecked(bool service_is_available);

  // Callback for DebugDaemonClient::EnableDebuggingFeatures().
  void OnEnableDebuggingFeatures(bool success);

  // Callback for DebugDaemonClient::QueryDebuggingFeatures().
  void OnQueryDebuggingFeatures(bool success, int features_flag);

  // Callback for DebugDaemonClient::RemoveRootfsVerification().
  void OnRemoveRootfsVerification(bool success);

  // Updates UI state.
  void UpdateUIState(UIState state);

  Delegate* delegate_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  base::WeakPtrFactory<EnableDebuggingScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnableDebuggingScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
