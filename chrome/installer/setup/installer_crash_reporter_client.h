// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTER_CLIENT_H_
#define CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTER_CLIENT_H_

#include <stddef.h>

#include "base/macros.h"
#include "components/crash/content/app/crash_reporter_client.h"

class InstallerCrashReporterClient
    : public crash_reporter::CrashReporterClient {
 public:
  explicit InstallerCrashReporterClient(bool is_per_user_install);
  ~InstallerCrashReporterClient() override;

  // crash_reporter::CrashReporterClient methods:
  bool ShouldCreatePipeName(const base::string16& process_type) override;
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
  bool ReportingIsEnforcedByPolicy(bool* enabled) override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  bool is_per_user_install_;

  DISALLOW_COPY_AND_ASSIGN(InstallerCrashReporterClient);
};

#endif  // CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTER_CLIENT_H_
