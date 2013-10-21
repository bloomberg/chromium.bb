// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_paths.h"

#include "base/files/file_path.h"
#include "base/path_service.h"

namespace chromeos {

namespace {

const base::FilePath::CharType kDefaultAppOrderFileName[] =
#if defined(GOOGLE_CHROME_BUILD)
    FILE_PATH_LITERAL("/usr/share/google-chrome/default_app_order.json");
#else
    FILE_PATH_LITERAL("/usr/share/chromium/default_app_order.json");
#endif  // defined(GOOGLE_CHROME_BUILD)

const base::FilePath::CharType kDefaultUserPolicyKeysDir[] =
    FILE_PATH_LITERAL("/var/run/user_policy");

const base::FilePath::CharType kOwnerKeyFileName[] =
    FILE_PATH_LITERAL("/var/lib/whitelist/owner.key");

const base::FilePath::CharType kInstallAttributesFileName[] =
    FILE_PATH_LITERAL("/var/run/lockbox/install_attributes.pb");

const base::FilePath::CharType kUptimeFileName[] =
    FILE_PATH_LITERAL("/proc/uptime");

const base::FilePath::CharType kUpdateRebootNeededUptimeFile[] =
    FILE_PATH_LITERAL("/var/run/chrome/update_reboot_needed_uptime");

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case FILE_DEFAULT_APP_ORDER:
      *result = base::FilePath(kDefaultAppOrderFileName);
      break;
    case DIR_USER_POLICY_KEYS:
      *result = base::FilePath(kDefaultUserPolicyKeysDir);
      break;
    case FILE_OWNER_KEY:
      *result = base::FilePath(kOwnerKeyFileName);
      break;
    case FILE_INSTALL_ATTRIBUTES:
      *result = base::FilePath(kInstallAttributesFileName);
      break;
    case FILE_UPTIME:
      *result = base::FilePath(kUptimeFileName);
      break;
    case FILE_UPDATE_REBOOT_NEEDED_UPTIME:
      *result = base::FilePath(kUpdateRebootNeededUptimeFile);
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace chromeos
