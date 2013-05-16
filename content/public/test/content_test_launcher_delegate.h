// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_TEST_LAUNCHER_DELEGATE_H_
#define CONTENT_PUBLIC_TEST_CONTENT_TEST_LAUNCHER_DELEGATE_H_

#include "content/public/test/test_launcher.h"

namespace content {

class ContentTestLauncherDelegate : public TestLauncherDelegate {
 public:
  ContentTestLauncherDelegate();
  virtual ~ContentTestLauncherDelegate();

  // TestLauncherDelegate:
  virtual std::string GetEmptyTestName() OVERRIDE;
  virtual int RunTestSuite(int argc, char** argv) OVERRIDE;
  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line,
      const base::FilePath& temp_data_dir) OVERRIDE;
  virtual ContentMainDelegate* CreateContentMainDelegate() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentTestLauncherDelegate);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_TEST_LAUNCHER_DELEGATE_H_
