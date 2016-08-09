// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_
#define ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_

#include <map>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/service.h"

namespace views {
class AuraInit;
class Widget;
class WindowManagerConnection;
}

namespace ash {
namespace touch_hud {

class TouchHudApplication
    : public shell::Service,
      public mash::mojom::Launchable,
      public shell::InterfaceFactory<mash::mojom::Launchable> {
 public:
  TouchHudApplication();
  ~TouchHudApplication() override;

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override;

  // shell::InterfaceFactory<mojom::Launchable>:
  void Create(const shell::Identity& remote_identity,
              mash::mojom::LaunchableRequest request) override;

  mojo::Binding<mash::mojom::Launchable> binding_;
  views::Widget* widget_ = nullptr;

  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudApplication);
};

}  // namespace touch_hud
}  // namespace ash

#endif  // ASH_TOUCH_HUD_MUS_TOUCH_HUD_APPLICATION_H_
