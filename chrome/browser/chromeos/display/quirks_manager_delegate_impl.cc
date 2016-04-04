// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/quirks_manager_delegate_impl.h"

#include "base/path_service.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_paths.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"

namespace {

const char kUserDataDisplayProfilesDirectory[] = "display_profiles";

int GetDaysSinceOobeOnBlockingPool() {
  return chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation().InDays();
}

}  // namespace

namespace quirks {

std::string QuirksManagerDelegateImpl::GetApiKey() const {
  return google_apis::GetAPIKey();
}

base::FilePath QuirksManagerDelegateImpl::GetBuiltInDisplayProfileDirectory()
    const {
  base::FilePath path;
  if (!PathService::Get(chromeos::DIR_DEVICE_COLOR_CALIBRATION_PROFILES, &path))
    LOG(ERROR) << "Could not get system path for display calibration profiles.";
  return path;
}

// On chrome device, returns /var/cache/display_profiles.
// On Linux desktop, returns {DIR_USER_DATA}/display_profiles.
base::FilePath QuirksManagerDelegateImpl::GetDownloadDisplayProfileDirectory()
    const {
  base::FilePath directory;
  if (base::SysInfo::IsRunningOnChromeOS()) {
    PathService::Get(chromeos::DIR_DEVICE_DISPLAY_PROFILES, &directory);
  } else {
    PathService::Get(chrome::DIR_USER_DATA, &directory);
    directory = directory.Append(kUserDataDisplayProfilesDirectory);
  }
  return directory;
}

bool QuirksManagerDelegateImpl::DevicePolicyEnabled() const {
  bool quirks_enabled = true;
  chromeos::CrosSettings::Get()->GetBoolean(
      chromeos::kDeviceQuirksDownloadEnabled, &quirks_enabled);
  return quirks_enabled;
}

void QuirksManagerDelegateImpl::GetDaysSinceOobe(
    QuirksManager::DaysSinceOobeCallback callback) const {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&GetDaysSinceOobeOnBlockingPool), callback);
}

}  // namespace quirks
