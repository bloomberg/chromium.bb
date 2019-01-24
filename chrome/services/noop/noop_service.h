// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_NOOP_NOOP_SERVICE_H_
#define CHROME_SERVICES_NOOP_NOOP_SERVICE_H_

#include "chrome/services/noop/public/mojom/noop.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace chrome {

// No-op service that does nothing. Will be used to analyze memory usage of
// starting an extra process.
class NoopService : public service_manager::Service, public mojom::Noop {
 public:
  explicit NoopService(service_manager::mojom::ServiceRequest request);
  ~NoopService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void BindNoopRequest(mojom::NoopRequest request);

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::Noop> bindings_;

  DISALLOW_COPY_AND_ASSIGN(NoopService);
};

}  // namespace chrome

#endif  // CHROME_SERVICES_NOOP_NOOP_SERVICE_H_
