// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_
#define CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
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
#if !defined(OS_MACOSX) && !defined(OS_WIN)
  void SetCrashReporterClientIdFromGUID(
      const std::string& client_guid) override;
#endif
#if defined(OS_WIN)
  bool GetAlternativeCrashDumpLocation(base::FilePath* crash_dir) override;
  void GetProductNameAndVersion(const base::FilePath& exe_path,
                                base::string16* product_name,
                                base::string16* version,
                                base::string16* special_build,
                                base::string16* channel_name) override;
  bool ShouldShowRestartDialog(base::string16* title,
                               base::string16* message,
                               bool* is_rtl_locale) override;
  bool AboutToRestart() override;
  bool GetDeferredUploadsSupported(bool is_per_user_install) override;
  bool GetIsPerUserInstall(const base::FilePath& exe_path) override;
  bool GetShouldDumpLargerDumps(bool is_per_user_install) override;
  int GetResultCodeRespawnFailed() override;
  void InitBrowserCrashDumpsRegKey() override;
  void RecordCrashDumpAttempt(bool is_real_crash) override;
  void RecordCrashDumpAttemptResult(bool is_real_crash,
                                    bool succeeded) override;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  base::FilePath GetReporterLogFilename() override;
#endif

  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;

  size_t RegisterCrashKeys() override;

  bool IsRunningUnattended() override;

  bool GetCollectStatsConsent() override;

#if defined(OS_WIN) || defined(OS_MACOSX)
  bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled) override;
#endif

#if defined(OS_ANDROID)
  int GetAndroidMinidumpDescriptor() override;
#endif

  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
#if defined(OS_WIN)
  scoped_ptr<browser_watcher::CrashReportingMetrics> crash_reporting_metrics_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeCrashReporterClient);
};

#endif  // CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_
