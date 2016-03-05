// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/tests/connect/connect_test.mojom.h"

namespace mojo {
namespace shell {

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ConnectTestApp : public ShellClient,
                       public InterfaceFactory<test::mojom::ConnectTestService>,
                       public InterfaceFactory<test::mojom::StandaloneApp>,
                       public InterfaceFactory<test::mojom::BlockedInterface>,
                       public test::mojom::ConnectTestService,
                       public test::mojom::StandaloneApp,
                       public test::mojom::BlockedInterface {
 public:
  ConnectTestApp() {}
  ~ConnectTestApp() override {}

 private:
  // mojo::ShellClient:
  void Initialize(Connector* connector, const std::string& name,
                  uint32_t id, uint32_t user_id) override {
    connector_ = connector;
    name_ = name;
    id_ = id;
    userid_ = user_id;
    bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestApp::OnConnectionError,
                   base::Unretained(this)));
    standalone_bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestApp::OnConnectionError,
                   base::Unretained(this)));
  }
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<test::mojom::ConnectTestService>(this);
    connection->AddInterface<test::mojom::StandaloneApp>(this);
    connection->AddInterface<test::mojom::BlockedInterface>(this);

    uint32_t remote_id = mojom::Connector::kInvalidApplicationID;
    connection->GetRemoteApplicationID(&remote_id);
    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_local_name = connection->GetConnectionName();
    state->connection_remote_name = connection->GetRemoteApplicationName();
    state->connection_remote_userid = connection->GetRemoteUserID();
    state->connection_remote_id = remote_id;
    state->initialize_local_name = name_;
    state->initialize_id = id_;
    state->initialize_userid = userid_;
    connection->GetInterface(&caller_);
    caller_->ConnectionAccepted(std::move(state));

    return true;
  }
  void ShellConnectionLost() override {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(Connection* connection,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::StandaloneApp>:
  void Create(Connection* connection,
              test::mojom::StandaloneAppRequest request) override {
    standalone_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::BlockedInterface>:
  void Create(Connection* connection,
              test::mojom::BlockedInterfaceRequest request) override {
    blocked_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("APP");
  }

  // test::mojom::StandaloneApp:
  void ConnectToAllowedAppInBlockedPackage(
      const ConnectToAllowedAppInBlockedPackageCallback& callback) override {
    base::RunLoop run_loop;
    scoped_ptr<Connection> connection =
        connector_->Connect("mojo:connect_test_a");
    connection->SetRemoteInterfaceProviderConnectionErrorHandler(
        base::Bind(&ConnectTestApp::OnConnectionBlocked,
                   base::Unretained(this), callback, &run_loop));
    test::mojom::ConnectTestServicePtr test_service;
    connection->GetInterface(&test_service);
    test_service->GetTitle(
        base::Bind(&ConnectTestApp::OnGotTitle, base::Unretained(this),
                   callback, &run_loop));
    {
      // This message is dispatched as a task on the same run loop, so we need
      // to allow nesting in order to pump additional signals.
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      run_loop.Run();
    }
  }

  // test::mojom::BlockedInterface:
  void GetTitleBlocked(const GetTitleBlockedCallback& callback) override {
    callback.Run("Called Blocked Interface!");
  }

  void OnConnectionBlocked(
      const ConnectToAllowedAppInBlockedPackageCallback& callback,
      base::RunLoop* run_loop) {
    callback.Run("uninitialized");
    run_loop->Quit();
  }

  void OnGotTitle(
      const ConnectToAllowedAppInBlockedPackageCallback& callback,
      base::RunLoop* run_loop,
      const mojo::String& title) {
    callback.Run(title);
    run_loop->Quit();
  }

  void OnConnectionError() {
    if (bindings_.empty() && standalone_bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  Connector* connector_ = nullptr;
  std::string name_;
  uint32_t id_ = mojom::Connector::kInvalidApplicationID;
  uint32_t userid_ = mojom::Connector::kUserRoot;
  BindingSet<test::mojom::ConnectTestService> bindings_;
  BindingSet<test::mojom::StandaloneApp> standalone_bindings_;
  BindingSet<test::mojom::BlockedInterface> blocked_bindings_;
  test::mojom::ExposedInterfacePtr caller_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestApp);
};

}  // namespace shell
}  // namespace mojo


MojoResult MojoMain(MojoHandle shell_handle) {
  MojoResult rv = mojo::ApplicationRunner(
      new mojo::shell::ConnectTestApp).Run(shell_handle);
  return rv;
}
