// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/background/tests/test.mojom.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
namespace shell {

class TestClient : public ShellClient,
                   public InterfaceFactory<mojom::TestService>,
                   public mojom::TestService {
 public:
  TestClient() {}
  ~TestClient() override {}

 private:
  // ShellClient:
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface(this);
    return true;
  }
  bool ShellConnectionLost() override {
    return true;
  }

  // InterfaceFactory<mojom::TestService>:
  void Create(Connection* connection,
              InterfaceRequest<mojom::TestService> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::TestService
  void Test(const TestCallback& callback) override {
    callback.Run();
  }

  BindingSet<mojom::TestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace shell
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::shell::TestClient);
  return runner.Run(shell_handle);
}
