// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOCLICK_MUS_AUTOCLICK_APPLICATION_H_
#define ASH_AUTOCLICK_MUS_AUTOCLICK_APPLICATION_H_

#include <map>

#include "ash/autoclick/common/autoclick_controller_common.h"
#include "ash/autoclick/common/autoclick_controller_common_delegate.h"
#include "ash/autoclick/mus/public/interfaces/autoclick.mojom.h"
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
namespace autoclick {

class AutoclickApplication
    : public shell::Service,
      public mash::mojom::Launchable,
      public mojom::AutoclickController,
      public shell::InterfaceFactory<mash::mojom::Launchable>,
      public shell::InterfaceFactory<mojom::AutoclickController>,
      public AutoclickControllerCommonDelegate {
 public:
  AutoclickApplication();
  ~AutoclickApplication() override;

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override;

  // mojom::AutoclickController:
  void SetAutoclickDelay(uint32_t delay_in_milliseconds) override;

  // shell::InterfaceFactory<mojom::Launchable>:
  void Create(const shell::Identity& remote_identity,
              mash::mojom::LaunchableRequest request) override;

  // shell::InterfaceFactory<mojom::AutoclickController>:
  void Create(const shell::Identity& remote_identity,
              mojom::AutoclickControllerRequest request) override;

  // AutoclickControllerCommonDelegate:
  views::Widget* CreateAutoclickRingWidget(
      const gfx::Point& event_location) override;
  void UpdateAutoclickRingWidget(views::Widget* widget,
                                 const gfx::Point& event_location) override;
  void DoAutoclick(const gfx::Point& event_location,
                   const int mouse_event_flags) override;
  void OnAutoclickCanceled() override;

  mojo::Binding<mash::mojom::Launchable> launchable_binding_;
  mojo::Binding<mojom::AutoclickController> autoclick_binding_;

  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;
  std::unique_ptr<AutoclickControllerCommon> autoclick_controller_common_;
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickApplication);
};

}  // namespace autoclick
}  // namespace ash

#endif  // ASH_AUTOCLICK_MUS_AUTOCLICK_APPLICATION_H_
