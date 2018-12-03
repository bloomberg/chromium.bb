// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "components/mirroring/mojom/mirroring_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace mirroring {

class Session;

class COMPONENT_EXPORT(MIRRORING_SERVICE) MirroringService final
    : public service_manager::Service,
      public mojom::MirroringService {
 public:
  MirroringService(service_manager::mojom::ServiceRequest request,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~MirroringService() override;

 private:
  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool OnServiceManagerConnectionLost() override;

  // Handles the request to connect to this service.
  void Create(mojom::MirroringServiceRequest request);

  // mojom::MirroringService implementation.
  void Start(mojom::SessionParametersPtr params,
             const gfx::Size& max_resolution,
             mojom::SessionObserverPtr observer,
             mojom::ResourceProviderPtr resource_provider,
             mojom::CastMessageChannelPtr outbound_channel,
             mojom::CastMessageChannelRequest inbound_channel) override;

  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive service_keepalive_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::MirroringService> bindings_;
  std::unique_ptr<Session> session_;  // Current mirroring session.

  DISALLOW_COPY_AND_ASSIGN(MirroringService);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_
