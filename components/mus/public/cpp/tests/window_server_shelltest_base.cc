// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/window_server_shelltest_base.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/common/args.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_test.h"
#include "ui/gl/gl_switches.h"

namespace mus {

namespace {

const char kTestAppName[] = "mojo:mus_ws_unittests_app";

class WindowServerShellTestClient : public shell::test::ShellTestClient {
 public:
  explicit WindowServerShellTestClient(WindowServerShellTestBase* test)
      : ShellTestClient(test), test_(test) {}
  ~WindowServerShellTestClient() override {}

 private:
  // shell::test::ShellTestClient:
  bool AcceptConnection(shell::Connection* connection) override {
    return test_->AcceptConnection(connection);
  }

  WindowServerShellTestBase* test_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerShellTestClient);
};

void EnsureCommandLineSwitch(const std::string& name) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(name))
    cmd_line->AppendSwitch(name);
}

}  // namespace

WindowServerShellTestBase::WindowServerShellTestBase()
    : ShellTest(kTestAppName) {
  EnsureCommandLineSwitch(kUseTestConfig);
  EnsureCommandLineSwitch(switches::kOverrideUseGLWithOSMesaForTests);
}

WindowServerShellTestBase::~WindowServerShellTestBase() {}

scoped_ptr<shell::ShellClient> WindowServerShellTestBase::CreateShellClient() {
  return make_scoped_ptr(new WindowServerShellTestClient(this));
}

}  // namespace mus
