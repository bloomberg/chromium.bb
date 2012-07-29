// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/ui/idle_logout_dialog_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

const int64 kLoginIdleTimeout = 100; // seconds

static base::LazyInstance<KioskModeIdleLogout>
    g_kiosk_mode_idle_logout = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void KioskModeIdleLogout::Initialize() {
  g_kiosk_mode_idle_logout.Get();
}

KioskModeIdleLogout::KioskModeIdleLogout() {
  if (KioskModeSettings::Get()->is_initialized())
    Setup();
  else
    KioskModeSettings::Get()->Initialize(base::Bind(&KioskModeIdleLogout::Setup,
                                                    base::Unretained(this)));
}

void KioskModeIdleLogout::Setup() {
  if (UserManager::Get()->IsLoggedInAsDemoUser()) {
    // This means that we're recovering from a crash; user is already
    // logged in, go ahead and setup the notifications.
    // We might get notified twice for the same idle event, in case the
    // previous notification hasn't fired yet - but that's taken care of
    // in the IdleLogout dialog; if you try to show another while one is
    // still showing, it'll just be ignored.
    SetupIdleNotifications();
    RequestNextIdleNotification();
  } else {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources());
  }
}

void KioskModeIdleLogout::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    SetupIdleNotifications();
    RequestNextIdleNotification();
  }
}

void KioskModeIdleLogout::IdleNotify(int64 threshold) {
  // We're idle, next time we go active, we need to know so we can remove
  // the logout dialog if it's still up.
  RequestNextActiveNotification();

  IdleLogoutDialogView::ShowDialog();
}

void KioskModeIdleLogout::ActiveNotify() {
  // Before anything else, close the logout dialog to prevent restart
  IdleLogoutDialogView::CloseDialog();

  // Now that we're active, register a request for notification for
  // the next time we go idle.
  RequestNextIdleNotification();
}

void KioskModeIdleLogout::SetupIdleNotifications() {
  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (!power_manager->HasObserver(this))
    power_manager->AddObserver(this);
}

void KioskModeIdleLogout::RequestNextActiveNotification() {
  DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestActiveNotification();
}

void KioskModeIdleLogout::RequestNextIdleNotification() {
  DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestIdleNotification(KioskModeSettings::Get()->
          GetIdleLogoutTimeout().InMilliseconds());
}

}  // namespace chromeos
