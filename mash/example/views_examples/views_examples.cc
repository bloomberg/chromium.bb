// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/tracing/public/cpp/provider.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"

namespace views {
class AuraInit;
class WindowManagerConnection;
}

class ViewsExamples
    : public shell::Service,
      public mash::mojom::Launchable,
      public shell::InterfaceFactory<mash::mojom::Launchable> {
 public:
  ViewsExamples() {}
  ~ViewsExamples() override {}

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override {
    tracing_.Initialize(connector(), identity.name());
    aura_init_.reset(
        new views::AuraInit(connector(), "views_mus_resources.pak"));
    window_manager_connection_ =
        views::WindowManagerConnection::Create(connector(), identity);
  }
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override {
    registry->AddInterface<mash::mojom::Launchable>(this);
    return true;
  }

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override {
    views::examples::ShowExamplesWindow(views::examples::QUIT_ON_CLOSE,
                                        nullptr, nullptr);
  }

  // shell::InterfaceFactory<mash::mojom::Launchable>:
  void Create(const shell::Identity& remote_identity,
              mash::mojom::LaunchableRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::BindingSet<mash::mojom::Launchable> bindings_;

  tracing::Provider tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(ViewsExamples);
};

MojoResult ServiceMain(MojoHandle service_request_handle) {
  return shell::ServiceRunner(new ViewsExamples).Run(service_request_handle);
}
