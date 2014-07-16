// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/kiosk_auto_launcher_session_manager_delegate.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

KioskAutoLauncherSessionManagerDelegate::
    KioskAutoLauncherSessionManagerDelegate() {
}

KioskAutoLauncherSessionManagerDelegate::
    ~KioskAutoLauncherSessionManagerDelegate() {
}

void KioskAutoLauncherSessionManagerDelegate::Start() {
  // Kiosk app launcher starts with login state.
  session_manager_->SetSessionState(
      session_manager::SESSION_STATE_LOGIN_PRIMARY);

  ShowLoginWizard(chromeos::WizardController::kAppLaunchSplashScreenName);

  // Login screen is skipped but 'login-prompt-visible' signal is still needed.
  VLOG(1) << "Kiosk app auto launch >> login-prompt-visible";
  DBusThreadManager::Get()->GetSessionManagerClient()->EmitLoginPromptVisible();
}

}  // namespace chromeos
