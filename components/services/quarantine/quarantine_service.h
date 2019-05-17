// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_SERVICE_H_
#define COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace quarantine {

class QuarantineService : public service_manager::Service {
 public:
  explicit QuarantineService(service_manager::mojom::ServiceRequest request);
  ~QuarantineService() override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  service_manager::ServiceBinding binding_;
  service_manager::ServiceKeepalive keepalive_;
  service_manager::BinderRegistry registry_;

  base::WeakPtrFactory<QuarantineService> weak_factory_{this};
};

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_SERVICE_H_
