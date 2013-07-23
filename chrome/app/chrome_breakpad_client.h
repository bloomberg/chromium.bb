// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_BREAKPAD_CLIENT_H_
#define CHROME_APP_CHROME_BREAKPAD_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/breakpad/breakpad_client.h"

namespace chrome {

class ChromeBreakpadClient : public breakpad::BreakpadClient {
 public:
  ChromeBreakpadClient();
  virtual ~ChromeBreakpadClient();

  // breakpad::BreakpadClient implementation.
#if defined(OS_WIN)
  virtual bool GetAlternativeCrashDumpLocation(base::FilePath* crash_dir)
      OVERRIDE;
  virtual void GetProductNameAndVersion(const base::FilePath& exe_path,
                                        base::string16* product_name,
                                        base::string16* version,
                                        base::string16* special_build) OVERRIDE;
  virtual bool ShouldShowRestartDialog(base::string16* title,
                                       base::string16* message,
                                       bool* is_rtl_locale) OVERRIDE;
  virtual bool AboutToRestart() OVERRIDE;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
  virtual void GetProductNameAndVersion(std::string* product_name,
                                        std::string* version) OVERRIDE;
  virtual base::FilePath GetReporterLogFilename() OVERRIDE;
#endif

  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir) OVERRIDE;

#if defined(OS_POSIX)
  virtual void SetDumpWithoutCrashingFunction(void (*function)()) OVERRIDE;
#endif

  virtual size_t RegisterCrashKeys() OVERRIDE;

  virtual bool IsRunningUnattended() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBreakpadClient);
};

}  // namespace chrome

#endif  // CHROME_APP_CHROME_BREAKPAD_CLIENT_H_
