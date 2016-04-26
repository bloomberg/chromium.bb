// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_MOJO_TEST_CONNECTOR_H_
#define CHROME_TEST_BASE_MOJO_TEST_CONNECTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/test/launcher/test_launcher.h"
#include "services/shell/background/background_shell.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace base {
class CommandLine;
}

namespace content {
class TestState;
}

namespace mojo {
class ShellClient;
class ShellConnection;
}

// MojoTestConnector in responsible for providing the necessary wiring for
// test processes to get a mojo channel passed to them.  To use this class
// call PrepareForTest() prior to launching each test. It is expected
// PrepareForTest() is called from content::TestLauncherDelegate::PreRunTest().
class MojoTestConnector {
 public:
  // Switch added to command line of each test.
  static const char kTestSwitch[];

  MojoTestConnector();
  ~MojoTestConnector();

  // Initializes the background thread the Shell runs on.
  shell::mojom::ShellClientRequest Init();

  std::unique_ptr<content::TestState> PrepareForTest(
      base::CommandLine* command_line,
      base::TestLauncher::LaunchOptions* test_launch_options);

 private:
  shell::BackgroundShell background_shell_;

  DISALLOW_COPY_AND_ASSIGN(MojoTestConnector);
};

#endif  // CHROME_TEST_BASE_MOJO_TEST_CONNECTOR_H_
