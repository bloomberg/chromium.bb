// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/runner/child/test_native_main.h"
#include "mojo/shell/runner/child/test_native_service.mojom.h"
#include "mojo/shell/runner/init.h"

namespace {

class TargetApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::shell::test::TestNativeService,
      public mojo::InterfaceFactory<mojo::shell::test::TestNativeService> {
 public:
  TargetApplicationDelegate() {}
  ~TargetApplicationDelegate() override {}

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override {}
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mojo::shell::test::TestNativeService>(this);
    return true;
  }

  // mojo::shell::test::TestNativeService:
  void Invert(bool from_driver, const InvertCallback& callback) override {
    callback.Run(!from_driver);
  }

  // mojo::InterfaceFactory<mojo::shell::test::TestNativeService>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::shell::test::TestNativeService>
                  request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::WeakBindingSet<mojo::shell::test::TestNativeService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TargetApplicationDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  TargetApplicationDelegate delegate;
  return mojo::shell::TestNativeMain(&delegate);
}
