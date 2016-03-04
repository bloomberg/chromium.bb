// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_TESTS_LIFECYCLE_APP_CLIENT_H_
#define MOJO_SHELL_TESTS_LIFECYCLE_APP_CLIENT_H_

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "mojo/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

using LifecycleControl = mojo::shell::test::mojom::LifecycleControl;
using LifecycleControlRequest =
    mojo::shell::test::mojom::LifecycleControlRequest;

namespace mojo {
class ShellConnection;
namespace shell {
namespace test {

class AppClient : public ShellClient,
                  public InterfaceFactory<LifecycleControl>,
                  public LifecycleControl {
 public:
  AppClient();
  explicit AppClient(shell::mojom::ShellClientRequest request);
  ~AppClient() override;

  void set_runner(ApplicationRunner* runner) {
    runner_ = runner;
  }

  // ShellClient:
  bool AcceptConnection(Connection* connection) override;

  // InterfaceFactory<LifecycleControl>:
  void Create(Connection* connection, LifecycleControlRequest request) override;

  // LifecycleControl:
  void Ping(const PingCallback& callback) override;
  void GracefulQuit() override;
  void Crash() override;
  void CloseShellConnection() override;

 private:
  void BindingLost();

  ApplicationRunner* runner_ = nullptr;
  BindingSet<LifecycleControl> bindings_;
  scoped_ptr<ShellConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(AppClient);
};

}  // namespace test
}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_TESTS_LIFECYCLE_APP_CLIENT_H_
