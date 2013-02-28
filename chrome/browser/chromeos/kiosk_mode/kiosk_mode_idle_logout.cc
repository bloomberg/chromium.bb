// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"

#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/ui/idle_logout_dialog_view.h"
#include "chrome/common/chrome_notification_types.h"
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
  if (KioskModeSettings::Get()->is_initialized()) {
    Setup();
  } else {
    KioskModeSettings::Get()->Initialize(base::Bind(&KioskModeIdleLogout::Setup,
                                                    base::Unretained(this)));
  }
}

KioskModeIdleLogout::~KioskModeIdleLogout() {
  if (ash::Shell::HasInstance() &&
      ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this))
    ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);
}

void KioskModeIdleLogout::Setup() {
  if (UserManager::Get()->IsLoggedInAsDemoUser()) {
    // This means that we're recovering from a crash.  The user is already
    // logged in, so go ahead and start the timer.
    StartTimer();
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
    StartTimer();
    registrar_.RemoveAll();
  }
}

void KioskModeIdleLogout::OnUserActivity() {
  IdleLogoutDialogView::CloseDialog();
  timer_.Reset();
}

void KioskModeIdleLogout::StartTimer() {
  if (!ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this))
    ash::Shell::GetInstance()->user_activity_detector()->AddObserver(this);
  timer_.Start(FROM_HERE, KioskModeSettings::Get()->GetIdleLogoutTimeout(),
               base::Bind(&KioskModeIdleLogout::OnTimeout,
                          base::Unretained(this)));
}

void KioskModeIdleLogout::OnTimeout() {
  IdleLogoutDialogView::ShowDialog();
}

}  // namespace chromeos
