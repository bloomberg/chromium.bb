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

class ConnectTestClassApp
    : public ShellClient,
      public InterfaceFactory<test::mojom::ConnectTestService>,
      public InterfaceFactory<test::mojom::ClassInterface>,
      public test::mojom::ConnectTestService,
      public test::mojom::ClassInterface {
 public:
  ConnectTestClassApp() {}
  ~ConnectTestClassApp() override {}

 private:
  // mojo::ShellClient:
  void Initialize(Connector* connector, const Identity& identity,
                  uint32_t id) override {
    connector_ = connector;
    identity_ = identity;
  }
  bool AcceptConnection(Connection* connection) override {
    CHECK(connection->HasCapabilityClass("class"));
    connection->AddInterface<test::mojom::ConnectTestService>(this);
    connection->AddInterface<test::mojom::ClassInterface>(this);
    inbound_connections_.insert(connection);
    connection->SetConnectionLostClosure(
        base::Bind(&ConnectTestClassApp::OnConnectionError,
                   base::Unretained(this), connection));
    return true;
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(Connection* connection,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::ClassInterface>:
  void Create(Connection* connection,
              test::mojom::ClassInterfaceRequest request) override {
    class_interface_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("CLASS APP");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(identity_.instance());
  }

  // test::mojom::ClassInterface:
  void Ping(const PingCallback& callback) override {
    callback.Run("PONG");
  }

  void OnConnectionError(Connection* connection) {
    auto it = inbound_connections_.find(connection);
    DCHECK(it != inbound_connections_.end());
    inbound_connections_.erase(it);
    if (inbound_connections_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  Connector* connector_ = nullptr;
  Identity identity_;
  std::set<Connection*> inbound_connections_;
  BindingSet<test::mojom::ConnectTestService> bindings_;
  BindingSet<test::mojom::ClassInterface> class_interface_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestClassApp);
};

}  // namespace shell
}  // namespace mojo


MojoResult MojoMain(MojoHandle shell_handle) {
  MojoResult rv = mojo::ApplicationRunner(
      new mojo::shell::ConnectTestClassApp).Run(shell_handle);
  return rv;
}
