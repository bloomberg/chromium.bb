// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/browser_context_keyed_service_factories.h"

#include "apps/app_load_service_factory.h"
#include "apps/app_restore_service_factory.h"
#include "apps/app_window_geometry_cache.h"

namespace apps {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  apps::AppLoadServiceFactory::GetInstance();
  apps::AppRestoreServiceFactory::GetInstance();
  apps::AppWindowGeometryCache::Factory::GetInstance();
}

}  // namespace apps
