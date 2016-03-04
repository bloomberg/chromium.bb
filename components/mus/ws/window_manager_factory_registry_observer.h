// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_OBSERVER_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_OBSERVER_H_

namespace mus {
namespace ws {

class WindowManagerFactoryService;

class WindowManagerFactoryRegistryObserver {
 public:
  // Called when the mojom::WindowManagerFactory associated with |service| has
  // been obtained.
  virtual void OnWindowManagerFactorySet(
      WindowManagerFactoryService* service) = 0;

 protected:
  virtual ~WindowManagerFactoryRegistryObserver() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_OBSERVER_H_
