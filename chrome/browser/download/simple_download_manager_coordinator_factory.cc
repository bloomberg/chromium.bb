// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"

// static
SimpleDownloadManagerCoordinatorFactory*
SimpleDownloadManagerCoordinatorFactory::GetInstance() {
  static base::NoDestructor<SimpleDownloadManagerCoordinatorFactory> instance;
  return instance.get();
}

// static
download::SimpleDownloadManagerCoordinator*
SimpleDownloadManagerCoordinatorFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<download::SimpleDownloadManagerCoordinator*>(
      GetInstance()->GetServiceForKey(key, true));
}

SimpleDownloadManagerCoordinatorFactory::
    SimpleDownloadManagerCoordinatorFactory()
    : SimpleKeyedServiceFactory("SimpleDownloadManagerCoordinator",
                                SimpleDependencyManager::GetInstance()) {}

SimpleDownloadManagerCoordinatorFactory::
    ~SimpleDownloadManagerCoordinatorFactory() = default;

std::unique_ptr<KeyedService>
SimpleDownloadManagerCoordinatorFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<download::SimpleDownloadManagerCoordinator>();
}

SimpleFactoryKey* SimpleDownloadManagerCoordinatorFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}
