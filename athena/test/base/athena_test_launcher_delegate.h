// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_ATHENA_TEST_LAUNCHER_DELEGATE_H_
#define ATHENA_TEST_BASE_ATHENA_TEST_LAUNCHER_DELEGATE_H_

#include "content/public/test/test_launcher.h"

namespace athena {
namespace test {

class AthenaTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  int RunTestSuite(int argc, char** argv) override;
  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override;
  content::ContentMainDelegate* CreateContentMainDelegate() override;
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_BASE_ATHENA_TEST_LAUNCHER_DELEGATE_H_
