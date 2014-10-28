// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_service_registration_manager.h"

namespace extensions {

TestServiceRegistrationManager::TestServiceRegistrationManager() {
  ServiceRegistrationManager::SetServiceRegistrationManagerForTest(this);
}

TestServiceRegistrationManager::~TestServiceRegistrationManager() {
  ServiceRegistrationManager::SetServiceRegistrationManagerForTest(nullptr);
}

void TestServiceRegistrationManager::AddServiceToServiceRegistry(
    content::ServiceRegistry* service_registry,
    internal::ServiceFactoryBase* factory) {
  auto test_factory = test_factories_.find(factory->GetName());
  if (test_factory != test_factories_.end()) {
    test_factory->second->Register(service_registry);
  } else {
    ServiceRegistrationManager::AddServiceToServiceRegistry(service_registry,
                                                            factory);
  }
}

}  // namespace extensions
