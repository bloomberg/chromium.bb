// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
#define CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_

#include "base/compiler_specific.h"
#include "components/crash/app/crash_reporter_client.h"

namespace content {

class ShellCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  ShellCrashReporterClient();
  virtual ~ShellCrashReporterClient();

#if defined(OS_WIN)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(const base::FilePath& exe_path,
                                        base::string16* product_name,
                                        base::string16* version,
                                        base::string16* special_build,
                                        base::string16* channel_name) OVERRIDE;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(std::string* product_name,
                                        std::string* version) OVERRIDE;

  virtual base::FilePath GetReporterLogFilename() OVERRIDE;
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir) OVERRIDE;

#if defined(OS_ANDROID)
  // Returns the descriptor key of the android minidump global descriptor.
  virtual int GetAndroidMinidumpDescriptor() OVERRIDE;
#endif

  virtual bool EnableBreakpadForProcess(
      const std::string& process_type) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellCrashReporterClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
