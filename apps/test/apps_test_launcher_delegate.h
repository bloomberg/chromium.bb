// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_TEST_APPS_TEST_LAUNCHER_DELEGATE_H_
#define APPS_TEST_APPS_TEST_LAUNCHER_DELEGATE_H_

#include "content/public/test/test_launcher.h"

namespace apps {

class AppsTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  virtual int RunTestSuite(int argc, char** argv) OVERRIDE;
  virtual bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) OVERRIDE;
  virtual content::ContentMainDelegate* CreateContentMainDelegate() OVERRIDE;
};

}  // namespace apps

#endif  // APPS_TEST_APPS_TEST_LAUNCHER_DELEGATE_H_
