// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/runner/child/test_native_main.h"
#include "mojo/shell/runner/init.h"
#include "mojo/shell/tests/connect/connect_test.mojom.h"

using mojo::shell::test::mojom::ConnectTestService;
using mojo::shell::test::mojom::ConnectTestServiceRequest;

namespace {

class Target : public mojo::ShellClient,
               public mojo::InterfaceFactory<ConnectTestService>,
               public ConnectTestService {
 public:
  Target() {}
  ~Target() override {}

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override {
    identity_ = identity;
  }
  bool AcceptConnection(mojo::Connection* connection) override {
    connection->AddInterface<ConnectTestService>(this);
    return true;
  }

  // mojo::InterfaceFactory<ConnectTestService>:
  void Create(mojo::Connection* connection,
              ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("connect_test_exe");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(identity_.instance());
  }

  mojo::Identity identity_;
  mojo::BindingSet<ConnectTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  Target target;
  return mojo::shell::TestNativeMain(&target);
}
