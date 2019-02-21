// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_DEPENDENCY_MANAGER_H_
#define COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_DEPENDENCY_MANAGER_H_

#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/keyed_service_export.h"

class SimpleFactoryKey;

// A singleton that listens for owners of SimpleFactoryKey' destruction
// notifications and rebroadcasts them to each SimpleKeyedBaseFactory in a safe
// order based on the stated dependencies by each service.
class KEYED_SERVICE_EXPORT SimpleDependencyManager : public DependencyManager {
 public:
  SimpleDependencyManager();

  // Called by each owners of SimpleFactoryKey before it is destroyed in order
  // to destroy all services associated with |key|.
  void DestroyKeyedServices(SimpleFactoryKey* key);

  static SimpleDependencyManager* GetInstance();

 private:
  ~SimpleDependencyManager() override;

#ifndef NDEBUG
  // DependencyManager:
  void DumpContextDependencies(void* context) const final;
#endif  // NDEBUG
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_DEPENDENCY_MANAGER_H_
