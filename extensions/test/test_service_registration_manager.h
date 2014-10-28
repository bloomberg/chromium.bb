// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_SERVICE_REGISTRATION_MANAGER_H_
#define EXTENSIONS_TEST_TEST_SERVICE_REGISTRATION_MANAGER_H_

#include <map>
#include <string>

#include "extensions/browser/mojo/service_registration_manager.h"

namespace extensions {

class TestServiceRegistrationManager : public ServiceRegistrationManager {
 public:
  TestServiceRegistrationManager();
  ~TestServiceRegistrationManager() override;

  // Overrides an existing service factory with |factory| for testing. This
  // does not alter the permission checks used to determine whether a service
  // is available.
  template <typename Interface>
  void OverrideServiceFactoryForTest(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>& factory) {
    linked_ptr<internal::ServiceFactoryBase> service_factory(
        new internal::ServiceFactory<Interface>(factory));
    auto result = test_factories_.insert(
        std::make_pair(Interface::Name_, service_factory));
    DCHECK(result.second);
  }

  void AddServiceToServiceRegistry(
      content::ServiceRegistry* service_registry,
      internal::ServiceFactoryBase* service_factory) override;

 private:
  std::map<std::string, linked_ptr<internal::ServiceFactoryBase>>
      test_factories_;

  ServiceRegistrationManager* service_registration_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceRegistrationManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_SERVICE_REGISTRATION_MANAGER_H_
