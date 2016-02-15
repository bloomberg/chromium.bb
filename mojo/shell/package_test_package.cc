// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/shell/package_test.mojom.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"

// Tests that multiple applications can be packaged in a single Mojo application
// implementing ContentHandler; that these applications can be specified by
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
                      mojom::ShellClientRequest request,
                      const Callback<void()>& destruct_callback)
      : base::SimpleThread(name),
        name_(name),
        request_(std::move(request)),
        destruct_callback_(destruct_callback),
        shell_(nullptr) {
    Start();
  }
  ~ProvidedShellClient() override {
    Join();
    destruct_callback_.Run();
  }

 private:
  // mojo::ShellClient:
  void Initialize(Shell* shell, const std::string& url, uint32_t id) override {
    shell_ = shell;
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
      shell_->Quit();
  }

  const std::string name_;
  mojom::ShellClientRequest request_;
  const Callback<void()> destruct_callback_;
  Shell* shell_;
  WeakBindingSet<test::mojom::PackageTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedShellClient);
};

class PackageTestShellClient
    : public ShellClient,
      public InterfaceFactory<mojom::ContentHandler>,
      public InterfaceFactory<test::mojom::PackageTestService>,
      public mojom::ContentHandler,
      public test::mojom::PackageTestService {
 public:
  PackageTestShellClient() : shell_(nullptr) {}
  ~PackageTestShellClient() override {}

 private:
  // mojo::ShellClient:
  void Initialize(Shell* shell, const std::string& url, uint32_t id) override {
    shell_ = shell;
    bindings_.set_connection_error_handler(
        base::Bind(&PackageTestShellClient::OnConnectionError,
                   base::Unretained(this)));
  }
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<ContentHandler>(this);
    connection->AddInterface<test::mojom::PackageTestService>(
        this);
    return true;
  }

  // InterfaceFactory<mojom::ContentHandler>:
  void Create(Connection* connection,
              mojom::ContentHandlerRequest request) override {
    content_handler_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::PackageTestService>:
  void Create(Connection* connection,
              test::mojom::PackageTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ContentHandler:
  void StartApplication(mojom::ShellClientRequest request,
                        URLResponsePtr response,
                        const Callback<void()>& destruct_callback) override {
    const std::string url = response->url;
    if (url == "mojo://package_test_a/")
      new ProvidedShellClient("A", std::move(request), destruct_callback);
    else if (url == "mojo://package_test_b/")
      new ProvidedShellClient("B", std::move(request), destruct_callback);
  }

  // test::mojom::PackageTestService:
  void GetName(const GetNameCallback& callback) override {
    callback.Run("ROOT");
  }

  void OnConnectionError() {
    if (bindings_.empty())
      shell_->Quit();
  }

  Shell* shell_;
  std::vector<scoped_ptr<ShellClient>> delegates_;
  WeakBindingSet<mojom::ContentHandler> content_handler_bindings_;
  WeakBindingSet<test::mojom::PackageTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(PackageTestShellClient);
};

}  // namespace shell
}  // namespace mojo


MojoResult MojoMain(MojoHandle shell_handle) {
  MojoResult rv = mojo::ApplicationRunner(
      new mojo::shell::PackageTestShellClient).Run(shell_handle);
  return rv;
}
