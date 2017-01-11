// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_reporter_client.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/policy_constants.h"

#if !defined(DISABLE_NACL)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "components/nacl/common/nacl_switches.h"
#include "native_client/src/trusted/service_runtime/osx/crash_filter.h"
#endif

bool ChromeCrashReporterClient::GetCrashMetricsLocation(
    base::FilePath* metrics_dir) {
  return PathService::Get(chrome::DIR_USER_DATA, metrics_dir);
}

bool ChromeCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* breakpad_enabled) {
  base::ScopedCFTypeRef<CFStringRef> key(
      base::SysUTF8ToCFStringRef(policy::key::kMetricsReportingEnabled));
  Boolean key_valid;
  Boolean metrics_reporting_enabled = CFPreferencesGetAppBooleanValue(key,
      kCFPreferencesCurrentApplication, &key_valid);
  if (key_valid &&
      CFPreferencesAppValueIsForced(key, kCFPreferencesCurrentApplication)) {
    *breakpad_enabled = metrics_reporting_enabled;
    return true;
  }
  return false;
}
