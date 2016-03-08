// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/runner/child/test_native_main.h"
#include "mojo/shell/runner/init.h"
#include "mojo/shell/tests/shell/shell_unittest.mojom.h"

using mojo::shell::test::mojom::CreateInstanceTestPtr;

namespace {

class Target : public mojo::ShellClient {
 public:
  Target() {}
  ~Target() override {}

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override {
    CreateInstanceTestPtr service;
    connector->ConnectToInterface("mojo:shell_unittest", &service);
    service->SetTargetID(id);
  }

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  Target target;
  return mojo::shell::TestNativeMain(&target);
}
