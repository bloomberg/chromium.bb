// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_MASH_MASH_CRASH_REPORTER_CLIENT_H_
#define CHROME_APP_MASH_MASH_CRASH_REPORTER_CLIENT_H_

#include "base/macros.h"
#include "components/crash/content/app/crash_reporter_client.h"

#if !defined(OS_CHROMEOS)
#error Unsupported platform
#endif

// Crash reporting support for chrome --mash. Used for the root process, ash,
// and other standalone services. Chrome content_browser handles its own crash
// reporting via ChromeCrashReporterClient.
// TODO(jamescook): Eliminate chrome dependencies from ChromeCrashReporterClient
// and share more code with this class.
class MashCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  MashCrashReporterClient();
  ~MashCrashReporterClient() override;

  // crash_reporter::CrashReporterClient implementation.
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  base::FilePath GetReporterLogFilename() override;
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
  size_t RegisterCrashKeys() override;
  bool IsRunningUnattended() override;
  bool GetCollectStatsConsent() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MashCrashReporterClient);
};

#endif  // CHROME_APP_MASH_MASH_CRASH_REPORTER_CLIENT_H_
