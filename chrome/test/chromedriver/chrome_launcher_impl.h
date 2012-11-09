// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome_launcher.h"

class Chrome;
class FilePath;
class Status;

class ChromeLauncherImpl : public ChromeLauncher {
 public:
  ChromeLauncherImpl();
  virtual ~ChromeLauncherImpl();

  // Overridden from ChromeLauncher:
  virtual Status Launch(const FilePath& chrome_exe,
                        scoped_ptr<Chrome>* chrome) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherImpl);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_IMPL_H_
