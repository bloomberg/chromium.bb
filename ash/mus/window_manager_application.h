// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
#define ASH_MUS_WINDOW_MANAGER_APPLICATION_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/common/types.h"

namespace base {
class SequencedWorkerPool;
}

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
class GpuService;
class WindowTreeClient;
}

namespace ash {
namespace mus {

class NativeWidgetFactoryMus;
class NetworkConnectDelegateMus;
class WindowManager;

// Hosts the window manager and the ash system user interface for mash.
class WindowManagerApplication
    : public service_manager::Service,
      public mash::session::mojom::ScreenlockStateListener {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  WindowManager* window_manager() { return window_manager_.get(); }

  mash::session::mojom::Session* session() { return session_.get(); }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  void InitWindowManager(
      std::unique_ptr<ui::WindowTreeClient> window_tree_client,
      const scoped_refptr<base::SequencedWorkerPool>& blocking_pool);

  // Initializes lower-level OS-specific components (e.g. D-Bus services).
  void InitializeComponents();
  void ShutdownComponents();

  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // session::mojom::ScreenlockStateListener:
  void ScreenlockStateChanged(bool locked) override;

  tracing::Provider tracing_;

  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<NativeWidgetFactoryMus> native_widget_factory_mus_;

  std::unique_ptr<ui::GpuService> gpu_service_;
  std::unique_ptr<views::SurfaceContextFactory> compositor_context_factory_;
  std::unique_ptr<WindowManager> window_manager_;

  // A blocking pool used by the WindowManager's shell; not used in tests.
  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  mash::session::mojom::SessionPtr session_;

  mojo::Binding<mash::session::mojom::ScreenlockStateListener>
      screenlock_state_listener_binding_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<NetworkConnectDelegateMus> network_connect_delegate_;
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
