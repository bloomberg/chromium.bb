// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/login_oobe_session_manager_delegate.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

LoginOobeSessionManagerDelegate::LoginOobeSessionManagerDelegate() {
}

LoginOobeSessionManagerDelegate::~LoginOobeSessionManagerDelegate() {
}

void LoginOobeSessionManagerDelegate::Start() {
  // State will be defined once out-of-box/login branching is complete.
  ShowLoginWizard(std::string());

  if (KioskModeSettings::Get()->IsKioskModeEnabled())
    InitializeKioskModeScreensaver();

  // Reset reboot after update flag when login screen is shown.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->ClearPref(prefs::kRebootAfterUpdate);
  }
}

}  // namespace chromeos
