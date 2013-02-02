// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_

#include "base/memory/scoped_ptr.h"

class Chrome;
class Status;

namespace base {
class FilePath;
}

// Launches Chrome. Must be thread safe.
class ChromeLauncher {
 public:
  virtual ~ChromeLauncher() {}

  // Launches Chrome found at the given path. If the path
  // is empty, the default Chrome binary is to be used.
  virtual Status Launch(const base::FilePath& chrome_exe,
                        scoped_ptr<Chrome>* chrome) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
