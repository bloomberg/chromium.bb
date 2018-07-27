// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
#define CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"

namespace content {

class ShellCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  ShellCrashReporterClient();
  ~ShellCrashReporterClient() override;

#if defined(OS_WIN)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  void GetProductNameAndVersion(const base::string16& exe_path,
                                base::string16* product_name,
                                base::string16* version,
                                base::string16* special_build,
                                base::string16* channel_name) override;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override;
  base::FilePath GetReporterLogFilename() override;
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
#if defined(OS_WIN)
  bool GetCrashDumpLocation(base::string16* crash_dir) override;
#else
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
#endif

#if defined(OS_ANDROID)
  // Returns the descriptor key of the android minidump global descriptor.
  int GetAndroidMinidumpDescriptor() override;
#endif

  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellCrashReporterClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
