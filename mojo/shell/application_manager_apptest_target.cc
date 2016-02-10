// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/shell/application_manager_apptests.mojom.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/runner/child/test_native_main.h"
#include "mojo/shell/runner/init.h"

using mojo::shell::test::mojom::CreateInstanceForHandleTestPtr;

namespace {

class TargetApplicationDelegate : public mojo::ShellClient {
 public:
  TargetApplicationDelegate() {}
  ~TargetApplicationDelegate() override {}

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override {
    CreateInstanceForHandleTestPtr service;
    shell->ConnectToInterface("mojo:mojo_shell_apptests", &service);
    service->SetTargetID(id);
  }

  DISALLOW_COPY_AND_ASSIGN(TargetApplicationDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  TargetApplicationDelegate delegate;
  return mojo::shell::TestNativeMain(&delegate);
}
