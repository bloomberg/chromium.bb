// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
#define ASH_MUS_WINDOW_MANAGER_APPLICATION_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "ash/public/cpp/config.h"
#include "ash/public/interfaces/wallpaper.mojom.h"
#include "ash/shell_delegate.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ui/common/types.h"

namespace aura {
class WindowTreeClient;
}

namespace chromeos {
namespace system {
class ScopedFakeStatisticsProvider;
}
}

namespace service_manager {
class Connector;
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
  // If |observer| is non-null it is added to the WindowManager once created.
  // See WindowManager's constructor for details of
  // |show_primary_host_on_connect|.
  explicit WindowManagerApplication(
      bool show_primary_host_on_connect,
      Config ash_config = Config::MASH,
      std::unique_ptr<ash::ShellDelegate> shell_delegate = nullptr);
  ~WindowManagerApplication() override;

  WindowManager* window_manager() { return window_manager_.get(); }

  service_manager::Connector* GetConnector();

 private:
  friend class ash::test::AshTestHelper;

  // If |init_network_handler| is true, chromeos::NetworkHandler is initialized.
  void InitWindowManager(
      std::unique_ptr<aura::WindowTreeClient> window_tree_client,
      bool init_network_handler);

  // Initializes lower-level OS-specific components (e.g. D-Bus services).
  void InitializeComponents(bool init_network_handler);
  void ShutdownComponents();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  const bool show_primary_host_on_connect_;

  std::unique_ptr<views::AuraInit> aura_init_;

  std::unique_ptr<WindowManager> window_manager_;

  std::unique_ptr<NetworkConnectDelegateMus> network_connect_delegate_;
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<ShellDelegate> shell_delegate_;

  const Config ash_config_;

  // Whether this class initialized NetworkHandler and needs to clean it up.
  bool network_handler_initialized_ = false;

  // Whether this class initialized DBusThreadManager and needs to clean it up.
  bool dbus_thread_manager_initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
