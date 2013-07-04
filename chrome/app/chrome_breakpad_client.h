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
#endif

  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBreakpadClient);
};

}  // namespace chrome

#endif  // CHROME_APP_CHROME_BREAKPAD_CLIENT_H_
