// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_
#define ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_

#include <map>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

namespace views {
class AuraInit;
class Widget;
}

namespace ash {
namespace touch_hud {

class TouchHudApplication
    : public service_manager::Service,
      public mash::mojom::Launchable,
      public service_manager::InterfaceFactory<mash::mojom::Launchable> {
 public:
  TouchHudApplication();
  ~TouchHudApplication() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override;

  // service_manager::InterfaceFactory<mojom::Launchable>:
  void Create(const service_manager::Identity& remote_identity,
              mash::mojom::LaunchableRequest request) override;

  mojo::Binding<mash::mojom::Launchable> binding_;
  views::Widget* widget_ = nullptr;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudApplication);
};

}  // namespace touch_hud
}  // namespace ash

#endif  // ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_
