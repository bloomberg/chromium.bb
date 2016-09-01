// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
#define ASH_MUS_WINDOW_MANAGER_APPLICATION_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "ash/public/interfaces/shelf.mojom.h"
#include "base/macros.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/accelerator_registrar.mojom.h"

namespace chromeos {
namespace system {
class ScopedFakeStatisticsProvider;
}
}

namespace views {
class AuraInit;
class SurfaceContextFactory;
}

namespace ui {
class Event;
class GpuService;
class WindowTreeClient;
}

namespace ash {
namespace mus {

class AcceleratorRegistrarImpl;
class NativeWidgetFactoryMus;
class WindowManager;

// Hosts the window manager and the ash system user interface for mash.
// TODO(mash): Port ash_sysui's WallpaperController here.
class WindowManagerApplication
    : public shell::Service,
      public shell::InterfaceFactory<ash::mojom::ShelfController>,
      public shell::InterfaceFactory<ui::mojom::AcceleratorRegistrar>,
      public mash::session::mojom::ScreenlockStateListener {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  WindowManager* window_manager() { return window_manager_.get(); }

  mash::session::mojom::Session* session() { return session_.get(); }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  void OnAcceleratorRegistrarDestroyed(AcceleratorRegistrarImpl* registrar);

  void InitWindowManager(ui::WindowTreeClient* window_tree_client);

  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // InterfaceFactory<ash::mojom::ShelfController>:
  void Create(const shell::Identity& remote_identity,
              ash::mojom::ShelfControllerRequest request) override;

  // shell::InterfaceFactory<ui::mojom::AcceleratorRegistrar>:
  void Create(const shell::Identity& remote_identity,
              ui::mojom::AcceleratorRegistrarRequest request) override;

  // session::mojom::ScreenlockStateListener:
  void ScreenlockStateChanged(bool locked) override;

  tracing::Provider tracing_;

  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<NativeWidgetFactoryMus> native_widget_factory_mus_;

  std::unique_ptr<ui::GpuService> gpu_service_;
  std::unique_ptr<views::SurfaceContextFactory> compositor_context_factory_;
  std::unique_ptr<WindowManager> window_manager_;

  mojo::BindingSet<ash::mojom::ShelfController> shelf_controller_bindings_;

  std::set<AcceleratorRegistrarImpl*> accelerator_registrars_;

  mash::session::mojom::SessionPtr session_;

  mojo::Binding<mash::session::mojom::ScreenlockStateListener>
      screenlock_state_listener_binding_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
