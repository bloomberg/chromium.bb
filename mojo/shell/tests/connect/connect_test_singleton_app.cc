// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mojo/public/c/system/main.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
namespace shell {

class ConnectTestSingletonApp : public ShellClient {
 public:
  ConnectTestSingletonApp() {}
  ~ConnectTestSingletonApp() override {}

 private:
  // mojo::ShellClient:
  void Initialize(Connector* connector, const Identity& identity,
                  uint32_t id) override {}
  bool AcceptConnection(Connection* connection) override {
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(ConnectTestSingletonApp);
};

}  // namespace shell
}  // namespace mojo


MojoResult MojoMain(MojoHandle shell_handle) {
  return mojo::ApplicationRunner(
      new mojo::shell::ConnectTestSingletonApp).Run(shell_handle);
}
