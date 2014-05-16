// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ui/idle_app_name_notification_view.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "extensions/browser/extension_system.h"
#include "ui/wm/core/user_activity_detector.h"

namespace chromeos {

namespace {

// The timeout in ms before the message shows up.
const int kIdleAppNameNotificationTimeoutMs = 2 * 60 * 1000;

// The duration of visibility for the message.
const int kMessageVisibilityTimeMs = 3000;

// The anomation time to show / hide the message.
const int kMessageAnimationTimeMs = 200;

// Our global instance of the Kiosk mode message.
KioskModeIdleAppNameNotification* g_kiosk_mode_idle_app_message = NULL;

}  // namespace

// static
void KioskModeIdleAppNameNotification::Initialize() {
  DCHECK(!g_kiosk_mode_idle_app_message);
  g_kiosk_mode_idle_app_message = new KioskModeIdleAppNameNotification();
}

// static
void KioskModeIdleAppNameNotification::Shutdown() {
  if (g_kiosk_mode_idle_app_message) {
    delete g_kiosk_mode_idle_app_message;
    g_kiosk_mode_idle_app_message = NULL;
  }
}

KioskModeIdleAppNameNotification::KioskModeIdleAppNameNotification()
    : show_notification_upon_next_user_activity_(false) {
  // Note: The timeout is currently fixed. If that changes we need to check if
  // the KioskModeSettings were already initialized.
  Setup();
}

KioskModeIdleAppNameNotification::~KioskModeIdleAppNameNotification() {
  if (ash::Shell::HasInstance() &&
      ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this)) {
    ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);
    // At this time the DBusThreadManager might already be gone.
    if (chromeos::DBusThreadManager::IsInitialized())
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient(
          )->RemoveObserver(this);
  }
}

void KioskModeIdleAppNameNotification::Setup() {
  DCHECK(UserManager::Get()->IsUserLoggedIn());
  Start();
}

void KioskModeIdleAppNameNotification::OnUserActivity(const ui::Event* event) {
  if (show_notification_upon_next_user_activity_) {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    const std::string app_id =
        command_line->GetSwitchValueASCII(::switches::kAppId);
    Profile* profile = ProfileManager::GetActiveUserProfile();
    notification_.reset(
        new IdleAppNameNotificationView(
            kMessageVisibilityTimeMs,
            kMessageAnimationTimeMs,
            extensions::ExtensionSystem::Get(profile
                )->extension_service()->GetInstalledExtension(app_id)));
    show_notification_upon_next_user_activity_ = false;
  }
  ResetTimer();
}

void KioskModeIdleAppNameNotification::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  // When we come back from a system resume we stop the timer and show the
  // message.
  timer_.Stop();
  OnTimeout();
}

void KioskModeIdleAppNameNotification::Start() {
  if (!ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this)) {
    ash::Shell::GetInstance()->user_activity_detector()->AddObserver(this);
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
        this);
  }
  ResetTimer();
}

void KioskModeIdleAppNameNotification::ResetTimer() {
  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    // OneShotTimer destroys the posted task after running it, so Reset()
    // isn't safe to call on a timer that's already fired.
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kIdleAppNameNotificationTimeoutMs),
        base::Bind(&KioskModeIdleAppNameNotification::OnTimeout,
                   base::Unretained(this)));
  }
}

void KioskModeIdleAppNameNotification::OnTimeout() {
  show_notification_upon_next_user_activity_ = true;
}

}  // namespace chromeos
