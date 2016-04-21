// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_
#define CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_

#if !defined(OS_WIN)

#include <stddef.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"

namespace browser_watcher {
class CrashReportingMetrics;
}  // namespace browser_watcher

class ChromeCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  ChromeCrashReporterClient();
  ~ChromeCrashReporterClient() override;

  // crash_reporter::CrashReporterClient implementation.
#if !defined(OS_MACOSX)
  void SetCrashReporterClientIdFromGUID(
      const std::string& client_guid) override;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  base::FilePath GetReporterLogFilename() override;
#endif

  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;

  size_t RegisterCrashKeys() override;

  bool IsRunningUnattended() override;

  bool GetCollectStatsConsent() override;

#if defined(OS_MACOSX)
  bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled) override;
#endif

#if defined(OS_ANDROID)
  int GetAndroidMinidumpDescriptor() override;
#endif

  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeCrashReporterClient);
};

#endif  // OS_WIN

#endif  // CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_
