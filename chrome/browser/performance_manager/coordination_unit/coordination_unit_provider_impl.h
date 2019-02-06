// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_COORDINATION_UNIT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_COORDINATION_UNIT_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "chrome/browser/performance_manager/coordination_unit/coordination_unit_graph.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/mojom/coordination_unit_provider.mojom.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace performance_manager {

class CoordinationUnitProviderImpl
    : public resource_coordinator::mojom::CoordinationUnitProvider {
 public:
  CoordinationUnitProviderImpl(
      service_manager::ServiceKeepalive* service_keepalive,
      CoordinationUnitGraph* coordination_unit_graph);
  ~CoordinationUnitProviderImpl() override;

  void Bind(
      resource_coordinator::mojom::CoordinationUnitProviderRequest request,
      const service_manager::BindSourceInfo& source_info);

  void OnConnectionError(CoordinationUnitBase* coordination_unit);

  // Overridden from resource_coordinator::mojom::CoordinationUnitProvider:
  void CreateFrameCoordinationUnit(
      resource_coordinator::mojom::FrameCoordinationUnitRequest request,
      const resource_coordinator::CoordinationUnitID& id) override;
  void CreatePageCoordinationUnit(
      resource_coordinator::mojom::PageCoordinationUnitRequest request,
      const resource_coordinator::CoordinationUnitID& id) override;
  void CreateProcessCoordinationUnit(
      resource_coordinator::mojom::ProcessCoordinationUnitRequest request,
      const resource_coordinator::CoordinationUnitID& id) override;
  void GetSystemCoordinationUnit(
      resource_coordinator::mojom::SystemCoordinationUnitRequest request)
      override;

 private:
  service_manager::ServiceKeepalive* const service_keepalive_;
  std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref_;
  CoordinationUnitGraph* coordination_unit_graph_;
  mojo::BindingSet<resource_coordinator::mojom::CoordinationUnitProvider>
      bindings_;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitProviderImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_COORDINATION_UNIT_PROVIDER_IMPL_H_
