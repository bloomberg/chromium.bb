// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/ui/screensaver_extension_dialog.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace {

// This (along with screensaver location) will change once we have the
// retail mode enterprise policy set up.
const int64 kScreensaverIdleTimeout = 60;

}  // namespace

namespace chromeos {

KioskModeScreensaver::KioskModeScreensaver() {
  // We should NOT be created if already logged in.
  CHECK(!chromeos::UserManager::Get()->user_is_logged_in());

  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());

  // We will register ourselves now and unregister if a user logs in.
  chromeos::PowerManagerClient* power_manager =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  if (!power_manager->HasObserver(this))
    power_manager->AddObserver(this);

  // Register for the next Idle for kScreensaverIdleTimeout event.
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestIdleNotification(kScreensaverIdleTimeout * 1000);
}

KioskModeScreensaver::~KioskModeScreensaver() {
  chromeos::PowerManagerClient* power_manager =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager->HasObserver(this))
    power_manager->RemoveObserver(this);
}

// NotificationObserver overrides:
void KioskModeScreensaver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_LOGIN_USER_CHANGED);
  // User logged in, remove our observers, screensaver will be deactivated.
  chromeos::PowerManagerClient* power_manager =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager->HasObserver(this))
    power_manager->RemoveObserver(this);

  ShutdownKioskModeScreensaver();
}

void KioskModeScreensaver::IdleNotify(int64 threshold) {
  // We're idle, next time we go active, we need to know so we can remove
  // the logout dialog if it's still up.
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestActiveNotification();

  browser::ShowScreensaverDialog();
}

void KioskModeScreensaver::ActiveNotify() {
  // Before anything else, close the logout dialog to prevent restart
  browser::CloseScreensaverDialog();

  // Now that we're active, register a request for notification for
  // the next time we go idle for kScreensaverIdleTimeout seconds.
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestIdleNotification(kScreensaverIdleTimeout * 1000);
}

static KioskModeScreensaver* g_kiosk_mode_screensaver = NULL;

void InitializeKioskModeScreensaver() {
  if (g_kiosk_mode_screensaver) {
    LOG(WARNING) << "Screensaver was already initialized";
    return;
  }

  g_kiosk_mode_screensaver = new KioskModeScreensaver();
}

void ShutdownKioskModeScreensaver() {
  if (g_kiosk_mode_screensaver) {
    LOG(WARNING) << "Screensaver shutdown called when uninitialized.";
    return;
  }

  delete g_kiosk_mode_screensaver;
  g_kiosk_mode_screensaver = NULL;
}

}  // namespace chromeos
