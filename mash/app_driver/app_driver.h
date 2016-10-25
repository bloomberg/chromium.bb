// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_APP_DRIVER_APP_DRIVER_H_
#define MASH_APP_DRIVER_APP_DRIVER_H_

#include <stdint.h>

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ui/public/interfaces/accelerator_registrar.mojom.h"

namespace mash {
namespace app_driver {

class AppDriver : public service_manager::Service,
                  public ui::mojom::AcceleratorHandler {
 public:
  AppDriver();
  ~AppDriver() override;

 private:
  void OnAvailableCatalogEntries(std::vector<catalog::mojom::EntryPtr> entries);

  // service_manager::Service:
  void OnStart(const service_manager::ServiceInfo& info) override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;
  bool OnStop() override;

  // ui::mojom::AcceleratorHandler:
  void OnAccelerator(uint32_t id, std::unique_ptr<ui::Event> event) override;

  void AddAccelerators();

  catalog::mojom::CatalogPtr catalog_;
  mojo::Binding<ui::mojom::AcceleratorHandler> binding_;
  base::WeakPtrFactory<AppDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppDriver);
};

}  // namespace app_driver
}  // namespace mash

#endif  // MASH_APP_DRIVER_APP_DRIVER_H_
