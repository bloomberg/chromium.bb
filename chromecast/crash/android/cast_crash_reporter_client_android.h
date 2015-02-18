// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_ANDROID_CAST_CRASH_REPORTER_CLIENT_ANDROID_H_
#define CHROMECAST_CRASH_ANDROID_CAST_CRASH_REPORTER_CLIENT_ANDROID_H_

#include "base/compiler_specific.h"
#include "components/crash/app/crash_reporter_client.h"

namespace chromecast {

class CastCrashReporterClientAndroid
    : public crash_reporter::CrashReporterClient {
 public:
  CastCrashReporterClientAndroid();
  ~CastCrashReporterClientAndroid() override;

  // crash_reporter::CrashReporterClient implementation:
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  base::FilePath GetReporterLogFilename() override;
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
  int GetAndroidMinidumpDescriptor() override;
  bool GetCollectStatsConsent() override;
  bool EnableBreakpadForProcess(
      const std::string& process_type) override;
  size_t RegisterCrashKeys() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastCrashReporterClientAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_ANDROID_CAST_CRASH_REPORTER_CLIENT_ANDROID_H_
