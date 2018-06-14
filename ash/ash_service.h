// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SERVICE_H_
#define ASH_ASH_SERVICE_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"

namespace service_manager {
struct EmbeddedServiceInfo;
}

namespace ash {

// Used to export Ash's mojo services, specifically the interfaces defined in
// Ash's manifest.json. Also responsible for creating the
// UI-Service/WindowService.
//
// NOTE: this is not used for --mash.
class ASH_EXPORT AshService : public service_manager::Service,
                              public service_manager::mojom::ServiceFactory {
 public:
  AshService();
  ~AshService() override;

  // Returns an appropriate EmbeddedServiceInfo that creates AshService.
  static service_manager::EmbeddedServiceInfo CreateEmbeddedServiceInfo();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle handle) override;

  // service_manager::mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest service,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override;

 private:
  void BindServiceFactory(
      service_manager::mojom::ServiceFactoryRequest request);

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(AshService);
};

}  // namespace ash

#endif  // ASH_ASH_SERVICE_H_
