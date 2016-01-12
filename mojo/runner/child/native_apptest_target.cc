// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/runner/child/test_native_main.h"
#include "mojo/runner/child/test_native_service.mojom.h"
#include "mojo/runner/init.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace {

class TargetApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::runner::test::TestNativeService,
      public mojo::InterfaceFactory<mojo::runner::test::TestNativeService> {
 public:
  TargetApplicationDelegate() {}
  ~TargetApplicationDelegate() override {}

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {}
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mojo::runner::test::TestNativeService>(this);
    return true;
  }

  // mojo::runner::test::TestNativeService:
  void Invert(bool from_driver, const InvertCallback& callback) override {
    callback.Run(!from_driver);
  }

  // mojo::InterfaceFactory<mojo::runner::test::TestNativeService>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::runner::test::TestNativeService>
                  request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::WeakBindingSet<mojo::runner::test::TestNativeService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TargetApplicationDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::runner::InitializeLogging();

  TargetApplicationDelegate delegate;
  return mojo::runner::TestNativeMain(&delegate);
}
