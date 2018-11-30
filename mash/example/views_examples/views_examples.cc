// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "mash/public/mojom/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/standalone_service/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/mus/aura_init.h"

class ViewsExamples : public service_manager::Service,
                      public mash::mojom::Launchable {
 public:
  explicit ViewsExamples(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface<mash::mojom::Launchable>(
        base::Bind(&ViewsExamples::Create, base::Unretained(this)));
  }
  ~ViewsExamples() override = default;

 private:
  // service_manager::Service:
  void OnStart() override {
    views::AuraInit::InitParams params;
    params.connector = service_binding_.GetConnector();
    params.identity = service_binding_.identity();
    aura_init_ = views::AuraInit::Create(params);
    if (!aura_init_)
      Terminate();
  }
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override {
    views::examples::ShowExamplesWindow(
        base::BindOnce(&ViewsExamples::Terminate, base::Unretained(this)));
  }

  void Create(mash::mojom::LaunchableRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  service_manager::ServiceBinding service_binding_;
  mojo::BindingSet<mash::mojom::Launchable> bindings_;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ViewsExamples);
};

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::MessageLoop message_loop;
  ViewsExamples(std::move(request)).RunUntilTermination();
}
