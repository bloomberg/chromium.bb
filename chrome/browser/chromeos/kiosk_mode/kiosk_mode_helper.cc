// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/common/chrome_switches.h"

namespace {

const int64 kScreensaverIdleTimeout = 60;
const int64 kLoginIdleTimeout = 100;
const int64 kLoginIdleCountdownTimeout = 20;

}  // namespace

namespace chromeos {

static base::LazyInstance<KioskModeHelper> g_kiosk_mode_helper =
    LAZY_INSTANCE_INITIALIZER;

// static
bool KioskModeHelper::IsKioskModeEnabled() {
  if (g_browser_process) {
    policy::BrowserPolicyConnector* bpc =
        g_browser_process->browser_policy_connector();
    if (bpc && policy::DEVICE_MODE_KIOSK == bpc->GetDeviceMode())
        return true;
  }
  // In case we've force-enabled kiosk mode.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableKioskMode))
    return true;

  return false;
}

// static
KioskModeHelper* KioskModeHelper::Get() {
  return g_kiosk_mode_helper.Pointer();
}

void KioskModeHelper::Initialize(const base::Closure& notify_initialized) {
  is_initialized_ = true;
  notify_initialized.Run();
}

std::string KioskModeHelper::GetScreensaverPath() const {
  if (!is_initialized_)
    return std::string();

  return CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kKioskModeScreensaverPath);
}

int64 KioskModeHelper::GetScreensaverTimeout() const {
  if (!is_initialized_)
    return -1;

  return kScreensaverIdleTimeout;
}

int64 KioskModeHelper::GetIdleLogoutTimeout() const {
  if (!is_initialized_)
    return -1;

  return kLoginIdleTimeout;
}
int64 KioskModeHelper::GetIdleLogoutWarningTimeout() const {
  if (!is_initialized_)
    return -1;

  return kLoginIdleCountdownTimeout;
}

KioskModeHelper::KioskModeHelper() : is_initialized_(false) {
}

}  // namespace chromeos
