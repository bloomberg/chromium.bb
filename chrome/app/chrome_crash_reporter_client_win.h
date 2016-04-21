// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_
#define CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_

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

  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;

  size_t RegisterCrashKeys() override;

  bool IsRunningUnattended() override;

  bool GetCollectStatsConsent() override;

  bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled) override;

  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  std::unique_ptr<browser_watcher::CrashReportingMetrics>
      crash_reporting_metrics_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCrashReporterClient);
};

#endif  // CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_
