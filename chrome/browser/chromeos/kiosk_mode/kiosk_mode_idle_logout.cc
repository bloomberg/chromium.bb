// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ui/idle_logout_dialog_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "ui/wm/core/user_activity_detector.h"

namespace chromeos {

namespace {

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
    Start();
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
    Start();
    registrar_.RemoveAll();
  }
}

void KioskModeIdleLogout::OnUserActivity(const ui::Event* event) {
  IdleLogoutDialogView::CloseDialog();
  ResetTimer();
}

void KioskModeIdleLogout::Start() {
  if (!ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this))
    ash::Shell::GetInstance()->user_activity_detector()->AddObserver(this);
  ResetTimer();
}

void KioskModeIdleLogout::ResetTimer() {
  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    // OneShotTimer destroys the posted task after running it, so Reset()
    // isn't safe to call on a timer that's already fired.
    timer_.Start(FROM_HERE, KioskModeSettings::Get()->GetIdleLogoutTimeout(),
                 base::Bind(&KioskModeIdleLogout::OnTimeout,
                            base::Unretained(this)));
  }
}

void KioskModeIdleLogout::OnTimeout() {
  IdleLogoutDialogView::ShowDialog();
}

}  // namespace chromeos
