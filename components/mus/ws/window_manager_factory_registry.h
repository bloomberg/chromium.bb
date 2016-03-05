// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "components/mus/ws/user_id_tracker_observer.h"

namespace mus {
namespace ws {

class ConnectionManager;
class UserIdTracker;
class WindowManagerFactoryRegistryObserver;
class WindowManagerFactoryService;

namespace test {
class WindowManagerFactoryRegistryTestApi;
}

// WindowManagerFactoryRegistry tracks the set of registered
// WindowManagerFactoryServices.
class WindowManagerFactoryRegistry : public UserIdTrackerObserver {
 public:
  explicit WindowManagerFactoryRegistry(ConnectionManager* connection_manager,
                                        UserIdTracker* tracker);
  ~WindowManagerFactoryRegistry() override;

  void Register(
      const UserId& user_id,
      mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request);

  std::vector<WindowManagerFactoryService*> GetServices();

  void AddObserver(WindowManagerFactoryRegistryObserver* observer);
  void RemoveObserver(WindowManagerFactoryRegistryObserver* observer);

 private:
  friend class WindowManagerFactoryService;
  friend class test::WindowManagerFactoryRegistryTestApi;

  void AddServiceImpl(scoped_ptr<WindowManagerFactoryService> service);

  bool ContainsServiceForUser(const UserId& user_id) const;
  void OnWindowManagerFactoryConnectionLost(
      WindowManagerFactoryService* service);
  void OnWindowManagerFactorySet(WindowManagerFactoryService* service);

  // UserIdTrackerObserver:
  void OnActiveUserIdChanged(const UserId& id) override;
  void OnUserIdAdded(const UserId& id) override;
  void OnUserIdRemoved(const UserId& id) override;

  // Set to true the first time a valid factory has been found.
  bool got_valid_factory_ = false;
  UserIdTracker* id_tracker_;
  ConnectionManager* connection_manager_;

  std::vector<scoped_ptr<WindowManagerFactoryService>> services_;

  base::ObserverList<WindowManagerFactoryRegistryObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerFactoryRegistry);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_H_
