// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/mash/mash_crash_reporter_client.h"

#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/sys_info.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/upload_list/crash_upload_list.h"
#include "components/version_info/version_info_values.h"

#if !defined(OS_CHROMEOS)
#error Unsupported platform
#endif

MashCrashReporterClient::MashCrashReporterClient() {}

MashCrashReporterClient::~MashCrashReporterClient() {}

void MashCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  *product_name = "Chrome_ChromeOS";
  *version = PRODUCT_VERSION;
}

base::FilePath MashCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(CrashUploadList::kReporterLogFilename);
}

bool MashCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  // For development on Linux desktop just use /tmp.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    *crash_dir = base::FilePath("/tmp");
    return true;
  }

  // TODO(jamescook): Refactor chrome::DIR_CRASH_DUMPS into //components/crash
  // and use it here.
  *crash_dir = base::FilePath("/var/log/chrome/Crash Reports");
  DCHECK(base::PathExists(*crash_dir));
  return true;
}

size_t MashCrashReporterClient::RegisterCrashKeys() {
  // Register keys used by non-browser processes launched by the service manager
  // (e.g. the GPU service). See chrome/common/crash_keys.cc.
  std::vector<base::debug::CrashKey> keys = {
      {"url-chunk", crash_keys::kLargeSize},
      {"total-discardable-memory-allocated", crash_keys::kSmallSize},
      {"discardable-memory-free", crash_keys::kSmallSize},
  };
  crash_keys::GetCrashKeysForCommandLineSwitches(&keys);
  return base::debug::InitCrashKeys(&keys.at(0), keys.size(),
                                    crash_keys::kChunkMaxLength);
}

bool MashCrashReporterClient::IsRunningUnattended() {
  return false;
}

bool MashCrashReporterClient::GetCollectStatsConsent() {
#if defined(GOOGLE_CHROME_BUILD)
  // For development on Linux desktop allow crash reporting to be forced on.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporterForTesting)) {
    return true;
  }
  // TODO(jamescook): Refactor this to share code with
  // chrome/browser/google/google_update_settings_posix.cc.
  base::FilePath consent_file("/home/chronos/Consent To Send Stats");
  return base::PathExists(consent_file);
#else
  // Crash reporting is only supported in branded builds.
  return false;
#endif  // defined(GOOGLE_CHROME_BUILD)
}
