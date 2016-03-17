// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

class Parent
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<mojo::shell::test::mojom::Parent>,
      public mojo::shell::test::mojom::Parent {
 public:
  Parent() {}
  ~Parent() override {
    connector_ = nullptr;
    child_connection_.reset();
    parent_bindings_.CloseAllBindings();
  }

 private:
  // ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override {
    connector_ = connector;
  }
  bool AcceptConnection(mojo::Connection* connection) override {
    connection->AddInterface<mojo::shell::test::mojom::Parent>(this);
    return true;
  }

  // InterfaceFactory<mojo::shell::test::mojom::Parent>:
  void Create(mojo::Connection* connection,
              mojo::shell::test::mojom::ParentRequest request) override {
    parent_bindings_.AddBinding(this, std::move(request));
  }

  // Parent:
  void ConnectToChild(const ConnectToChildCallback& callback) override {
    child_connection_ = connector_->Connect("mojo:lifecycle_unittest_app");
    mojo::shell::test::mojom::LifecycleControlPtr lifecycle;
    child_connection_->GetInterface(&lifecycle);
    {
      base::RunLoop loop;
      lifecycle->Ping(base::Bind(&QuitLoop, &loop));
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      loop.Run();
    }
    callback.Run();
  }
  void Quit() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

  mojo::Connector* connector_;
  scoped_ptr<mojo::Connection> child_connection_;
  mojo::BindingSet<mojo::shell::test::mojom::Parent> parent_bindings_;

  DISALLOW_COPY_AND_ASSIGN(Parent);
};

}  // namespace

MojoResult MojoMain(MojoHandle shell_handle) {
  Parent* parent = new Parent;
  return mojo::ApplicationRunner(parent).Run(shell_handle);
}
