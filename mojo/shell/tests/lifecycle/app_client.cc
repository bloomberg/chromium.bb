// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/tests/lifecycle/app_client.h"

#include "mojo/shell/public/cpp/shell_connection.h"

namespace mojo {
namespace shell {
namespace test {

AppClient::AppClient() {}
AppClient::AppClient(shell::mojom::ShellClientRequest request)
    : connection_(new ShellConnection(this, std::move(request))) {}
AppClient::~AppClient() {}

bool AppClient::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<LifecycleControl>(this);
  return true;
}

void AppClient::Create(mojo::Connection* connection,
                       LifecycleControlRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppClient::Ping(const PingCallback& callback) {
  callback.Run();
}

void AppClient::GracefulQuit() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void AppClient::Crash() {
  // Rather than actually crash, which causes a bunch of console spray and
  // maybe UI clutter on some platforms, just exit without shutting anything
  // down properly.
  exit(1);
}

void AppClient::CloseShellConnection() {
  DCHECK(runner_);
  runner_->DestroyShellConnection();
  // Quit the app once the caller goes away.
  bindings_.set_connection_error_handler(
      base::Bind(&AppClient::BindingLost, base::Unretained(this)));
}

void AppClient::BindingLost() {
  if (bindings_.empty())
    GracefulQuit();
}

}  // namespace test
}  // namespace shell
}  // namespace mojo


