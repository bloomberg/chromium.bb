// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_
#define ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_

#include <map>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace views {
class AuraInit;
class Widget;
}

namespace ash {
namespace touch_hud {

class TouchHudApplication : public service_manager::Service,
                            public mash::mojom::Launchable {
 public:
  TouchHudApplication();
  ~TouchHudApplication() override;

  void set_running_standalone(bool value) { running_standalone_ = value; }

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override;

  void Create(mash::mojom::LaunchableRequest request);

  service_manager::BinderRegistry registry_;
  mojo::Binding<mash::mojom::Launchable> binding_;
  views::Widget* widget_ = nullptr;

  std::unique_ptr<views::AuraInit> aura_init_;

  bool running_standalone_ = false;

  DISALLOW_COPY_AND_ASSIGN(TouchHudApplication);
};

}  // namespace touch_hud
}  // namespace ash

#endif  // ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_
