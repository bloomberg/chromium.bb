// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {
const int kDeviceModeFetchRetryDelayMs = 500;
}

const int KioskModeSettings::kMaxIdleLogoutTimeout = 600000;  // ms
const int KioskModeSettings::kMinIdleLogoutTimeout = 5000;  // ms

const int KioskModeSettings::kMaxIdleLogoutWarningDuration = 60000;  // ms
const int KioskModeSettings::kMinIdleLogoutWarningDuration = 1000;  // ms

static base::LazyInstance<KioskModeSettings> g_kiosk_mode_settings =
    LAZY_INSTANCE_INITIALIZER;

// static
KioskModeSettings* KioskModeSettings::Get() {
  return g_kiosk_mode_settings.Pointer();
}

bool KioskModeSettings::IsKioskModeEnabled() {
  return is_kiosk_mode_;
}

void KioskModeSettings::Initialize(const base::Closure& notify_initialized) {
  // No need to call more than once.
  if (is_initialized_) {
    notify_initialized.Run();
    return;
  }

  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings->PrepareTrustedValues(
      base::Bind(&KioskModeSettings::Initialize,
                 base::Unretained(this),
                 notify_initialized))) {
    return;
  }

  // Ignored till we land the code to pull the screensaver path from the app
  // packs with the screensaver id.
  cros_settings->GetString(kScreenSaverExtensionId, &screensaver_id_);

  int screensaver_timeout = 0;
  int idle_logout_timeout = 0;
  int idle_logout_warning_duration = 0;
  cros_settings->GetInteger(kScreenSaverTimeout, &screensaver_timeout);
  cros_settings->GetInteger(kIdleLogoutTimeout, &idle_logout_timeout);
  cros_settings->GetInteger(kIdleLogoutWarningDuration,
                            &idle_logout_warning_duration);

  // Restrict idle timeouts to safe values to prevent them from being turned off
  // or otherwise misused.
  idle_logout_timeout = std::min(idle_logout_timeout,
                                 KioskModeSettings::kMaxIdleLogoutTimeout);
  idle_logout_timeout = std::max(idle_logout_timeout,
                                 KioskModeSettings::kMinIdleLogoutTimeout);

  idle_logout_warning_duration =
      std::min(idle_logout_warning_duration,
               KioskModeSettings::kMaxIdleLogoutWarningDuration);
  idle_logout_warning_duration =
      std::max(idle_logout_warning_duration,
               KioskModeSettings::kMinIdleLogoutWarningDuration);

  screensaver_timeout_ = base::TimeDelta::FromMilliseconds(
      screensaver_timeout);
  idle_logout_timeout_ =
      base::TimeDelta::FromMilliseconds(idle_logout_timeout);
  idle_logout_warning_duration_ =
      base::TimeDelta::FromMilliseconds(idle_logout_warning_duration);

  is_initialized_ = true;
  notify_initialized.Run();
}

bool KioskModeSettings::is_initialized() const {
  return is_initialized_;
}

void KioskModeSettings::GetScreensaverPath(
    policy::AppPackUpdater::ScreenSaverUpdateCallback callback) const {
  if (!is_initialized_) {
    callback.Run(FilePath());
    return;
  }

  // Command line flag overrides policy since it can be used
  // for testing and dev workflows.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKioskModeScreensaverPath)) {
    callback.Run(FilePath(
        CommandLine::ForCurrentProcess()->
            GetSwitchValueASCII(switches::kKioskModeScreensaverPath)));
    return;
  }

  if (g_browser_process) {
    policy::BrowserPolicyConnector* bpc =
        g_browser_process->browser_policy_connector();
    if (bpc && bpc->GetAppPackUpdater()) {
      bpc->GetAppPackUpdater()->SetScreenSaverUpdateCallback(callback);
      return;
    }
  }
}

base::TimeDelta KioskModeSettings::GetScreensaverTimeout() const {
  if (!is_initialized_)
    return base::TimeDelta::FromSeconds(-1);

  return screensaver_timeout_;
}

base::TimeDelta KioskModeSettings::GetIdleLogoutTimeout() const {
  if (!is_initialized_)
    return base::TimeDelta::FromSeconds(-1);

  return idle_logout_timeout_;
}

base::TimeDelta KioskModeSettings::GetIdleLogoutWarningDuration() const {
  if (!is_initialized_)
    return base::TimeDelta::FromSeconds(-1);

  return idle_logout_warning_duration_;
}

KioskModeSettings::KioskModeSettings() : is_initialized_(false) {
  // In case we've force-enabled kiosk mode.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableKioskMode)) {
    is_kiosk_mode_ = true;
    return;
  }

  // Precache the value as we know it at construction time to avoid serving
  // different values to different users.
  if (g_browser_process) {
    policy::BrowserPolicyConnector* bpc =
        g_browser_process->browser_policy_connector();
    policy::DeviceMode device_mode = bpc->GetDeviceMode();
    if (device_mode == policy::DEVICE_MODE_KIOSK) {
      is_kiosk_mode_ = true;
      return;
    } else if (device_mode == policy::DEVICE_MODE_PENDING){
      content::BrowserThread::PostDelayedTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&KioskModeSettings::VerifyModeIsKnown,
                     base::Unretained(this)),
          kDeviceModeFetchRetryDelayMs);
    }
  }
  is_kiosk_mode_ = false;
}

KioskModeSettings::~KioskModeSettings() {
}

void KioskModeSettings::VerifyModeIsKnown() {
  if (g_browser_process) {
    policy::BrowserPolicyConnector* bpc =
        g_browser_process->browser_policy_connector();
    policy::DeviceMode device_mode = bpc->GetDeviceMode();
    // We retry asking for the mode until it becomes known.
    switch (device_mode) {
      case policy::DEVICE_MODE_PENDING:
        content::BrowserThread::PostDelayedTask(
            content::BrowserThread::UI, FROM_HERE,
            base::Bind(&KioskModeSettings::VerifyModeIsKnown,
                       base::Unretained(this)),
            kDeviceModeFetchRetryDelayMs);
        break;
      case policy::DEVICE_MODE_KIOSK:
        BrowserList::ExitCleanly();
        break;
      default:
        break;
    }
  }
}


}  // namespace chromeos
