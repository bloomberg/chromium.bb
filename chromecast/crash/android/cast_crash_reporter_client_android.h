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
  virtual ~CastCrashReporterClientAndroid();

  // crash_reporter::CrashReporterClient implementation:
  virtual void GetProductNameAndVersion(const char** product_name,
                                        const char** version) override;
  virtual base::FilePath GetReporterLogFilename() override;
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
  virtual int GetAndroidMinidumpDescriptor() override;
  virtual bool GetCollectStatsConsent() override;
  virtual bool EnableBreakpadForProcess(
      const std::string& process_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastCrashReporterClientAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_ANDROID_CAST_CRASH_REPORTER_CLIENT_ANDROID_H_
