// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/runner/child/test_native_main.h"
#include "mojo/shell/runner/init.h"
#include "mojo/shell/tests/connect/connect_test.mojom.h"
#include "mojo/shell/tests/util.h"

using mojo::shell::test::mojom::ClientProcessTest;
using mojo::shell::test::mojom::ClientProcessTestRequest;

namespace {

class Driver : public mojo::ShellClient,
               public mojo::InterfaceFactory<ClientProcessTest>,
               public ClientProcessTest {
 public:
  Driver() {}
  ~Driver() override {}

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override {
    connector_ = connector;
  }
  bool AcceptConnection(mojo::Connection* connection) override {
    connection->AddInterface<ClientProcessTest>(this);
    return true;
  }
  bool ShellConnectionLost() override {
    // TODO(rockot): http://crbug.com/596621. Should be able to remove this
    // override entirely.
    _exit(1);
  }

  // mojo::InterfaceFactory<ConnectTestService>:
  void Create(mojo::Connection* connection,
              ClientProcessTestRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ClientProcessTest:
  void LaunchAndConnectToProcess(
      const LaunchAndConnectToProcessCallback& callback) override {
    base::Process process;
    scoped_ptr<mojo::Connection> connection =
        mojo::shell::test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
            "connect_test_exe.exe",
#else
            "connect_test_exe",
#endif
            mojo::Identity("exe:connect_test_exe",
                           mojo::shell::mojom::kInheritUserID),
            connector_,
            &process);
    callback.Run(static_cast<int32_t>(connection->GetResult()),
                 mojo::shell::mojom::Identity::From(
                    connection->GetRemoteIdentity()));
  }

  mojo::Connector* connector_ = nullptr;
  mojo::BindingSet<ClientProcessTest> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Driver);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  Driver driver;
  return mojo::shell::TestNativeMain(&driver);
}
