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
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/common/types.h"

namespace aura {
class WindowTreeClient;
}

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
}

namespace ash {
namespace test {
class AshTestHelper;
}
namespace mus {

class NetworkConnectDelegateMus;
class WindowManager;

// Hosts the window manager and the ash system user interface for mash.
class WindowManagerApplication : public service_manager::Service {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  WindowManager* window_manager() { return window_manager_.get(); }

 private:
  friend class ash::test::AshTestHelper;
  friend class WmTestBase;
  friend class WmTestHelper;

  void InitWindowManager(
      std::unique_ptr<aura::WindowTreeClient> window_tree_client,
      const scoped_refptr<base::SequencedWorkerPool>& blocking_pool);

  // Initializes lower-level OS-specific components (e.g. D-Bus services).
  void InitializeComponents();
  void ShutdownComponents();

  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  tracing::Provider tracing_;

  std::unique_ptr<views::AuraInit> aura_init_;

  std::unique_ptr<WindowManager> window_manager_;

  // A blocking pool used by the WindowManager's shell; not used in tests.
  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  std::unique_ptr<NetworkConnectDelegateMus> network_connect_delegate_;
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
