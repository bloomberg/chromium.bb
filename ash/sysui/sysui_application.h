// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_SYSUI_APPLICATION_H_
#define ASH_SYSUI_SYSUI_APPLICATION_H_

#include <memory>

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/public/cpp/input_devices/input_device_client.h"

namespace ash {
namespace sysui {

class AshInit;

class SysUIApplication
    : public shell::Service,
      public shell::InterfaceFactory<mojom::WallpaperController> {
 public:
  SysUIApplication();
  ~SysUIApplication() override;

 private:
  // shell::Service:
  void OnStart(const ::shell::Identity& identity) override;
  bool OnConnect(const ::shell::Identity& remote_identity,
                 ::shell::InterfaceRegistry* registry) override;

  // InterfaceFactory<mojom::WallpaperController>:
  void Create(const shell::Identity& remote_identity,
              mojom::WallpaperControllerRequest request) override;

  tracing::Provider tracing_;
  std::unique_ptr<AshInit> ash_init_;

  mojo::BindingSet<mojom::WallpaperController> wallpaper_controller_bindings_;

  // Subscribes to updates about input-devices.
  ui::InputDeviceClient input_device_client_;

  DISALLOW_COPY_AND_ASSIGN(SysUIApplication);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_SYSUI_SYSUI_APPLICATION_H_
