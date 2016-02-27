// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"
#include "mojo/shell/tests/package_test.mojom.h"

// Tests that multiple applications can be packaged in a single Mojo application
// implementing ShellClientFactory; that these applications can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace mojo {
namespace shell {

using GetNameCallback = test::mojom::PackageTestService::GetNameCallback;

class ProvidedShellClient
    : public ShellClient,
      public InterfaceFactory<test::mojom::PackageTestService>,
      public test::mojom::PackageTestService,
      public base::SimpleThread {
 public:
  ProvidedShellClient(const std::string& name,
                      mojom::ShellClientRequest request)
      : base::SimpleThread(name),
        name_(name),
        request_(std::move(request)) {
    Start();
  }
  ~ProvidedShellClient() override {
    Join();
  }

 private:
  // mojo::ShellClient:
  void Initialize(Connector* connector, const std::string& name,
                  uint32_t id, uint32_t user_id) override {
    bindings_.set_connection_error_handler(
        base::Bind(&ProvidedShellClient::OnConnectionError,
                   base::Unretained(this)));
  }
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<test::mojom::PackageTestService>(
        this);
    return true;
  }

  // InterfaceFactory<test::mojom::PackageTestService>:
  void Create(Connection* connection,
              test::mojom::PackageTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::PackageTestService:
  void GetName(const GetNameCallback& callback) override {
    callback.Run(name_);
  }

  // base::SimpleThread:
  void Run() override {
    ApplicationRunner(this).Run(request_.PassMessagePipe().release().value(),
                                false);
    delete this;
  }

  void OnConnectionError() {
    if (bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  const std::string name_;
  mojom::ShellClientRequest request_;
  BindingSet<test::mojom::PackageTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedShellClient);
};

class PackageTestShellClient
    : public ShellClient,
      public InterfaceFactory<mojom::ShellClientFactory>,
      public InterfaceFactory<test::mojom::PackageTestService>,
      public mojom::ShellClientFactory,
      public test::mojom::PackageTestService {
 public:
  PackageTestShellClient() {}
  ~PackageTestShellClient() override {}

 private:
  // mojo::ShellClient:
  void Initialize(Connector* connector, const std::string& name,
                  uint32_t id, uint32_t user_id) override {
    bindings_.set_connection_error_handler(
        base::Bind(&PackageTestShellClient::OnConnectionError,
                   base::Unretained(this)));
  }
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<ShellClientFactory>(this);
    connection->AddInterface<test::mojom::PackageTestService>(this);
    return true;
  }
  bool ShellConnectionLost() override {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->QuitWhenIdle();
    }
    return true;
  }

  // InterfaceFactory<mojom::ShellClientFactory>:
  void Create(Connection* connection,
              mojom::ShellClientFactoryRequest request) override {
    shell_client_factory_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::PackageTestService>:
  void Create(Connection* connection,
              test::mojom::PackageTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ShellClientFactory:
  void CreateShellClient(mojom::ShellClientRequest request,
                         const String& name) override {
    if (name == "mojo:package_test_a")
      new ProvidedShellClient("A", std::move(request));
    else if (name == "mojo:package_test_b")
      new ProvidedShellClient("B", std::move(request));
  }

  // test::mojom::PackageTestService:
  void GetName(const GetNameCallback& callback) override {
    callback.Run("ROOT");
  }

  void OnConnectionError() {
    if (bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  std::vector<scoped_ptr<ShellClient>> delegates_;
  BindingSet<mojom::ShellClientFactory> shell_client_factory_bindings_;
  BindingSet<test::mojom::PackageTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(PackageTestShellClient);
};

}  // namespace shell
}  // namespace mojo


MojoResult MojoMain(MojoHandle shell_handle) {
  MojoResult rv = mojo::ApplicationRunner(
      new mojo::shell::PackageTestShellClient).Run(shell_handle);
  return rv;
}
