// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/session_state_controller_delegate_chromeos.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

SessionStateControllerDelegateChromeos::SessionStateControllerDelegateChromeos()
    : weak_factory_(this) {
}

SessionStateControllerDelegateChromeos::
    ~SessionStateControllerDelegateChromeos() {
}

bool SessionStateControllerDelegateChromeos::IsLoading() const {
  return ScreenLocker::default_screen_locker()
      ->GetAssociatedWebUI()
      ->GetWebContents()
      ->IsLoading();
}

void SessionStateControllerDelegateChromeos::RequestLockScreen() {
  // TODO(antrim) : additional logging for crbug/173178
  LOG(WARNING) << "Requesting screen lock from SessionStateControllerDelegate";
  DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
}

void SessionStateControllerDelegateChromeos::RequestShutdown() {
  CrosSettings* cros_settings = CrosSettings::Get();
  CrosSettingsProvider::TrustedStatus status =
      cros_settings->PrepareTrustedValues(base::Bind(
          &SessionStateControllerDelegateChromeos::RequestShutdown,
          weak_factory_.GetWeakPtr()));
  if (status != CrosSettingsProvider::TRUSTED)
    return;

  // Get the updated policy.
  bool reboot_on_shutdown = false;
  cros_settings->GetBoolean(kRebootOnShutdown, &reboot_on_shutdown);

  if (reboot_on_shutdown)
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  else
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
}

}  // namespace chromeos
