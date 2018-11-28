// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/app_registry_cache.h"

#include <utility>

namespace apps {

AppRegistryCache::AppRegistryCache() = default;

AppRegistryCache::~AppRegistryCache() = default;

void AppRegistryCache::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppRegistryCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppRegistryCache::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  for (const auto& delta : deltas) {
    auto iter = states_.find(delta->app_id);
    if (iter == states_.end()) {
      // The previous state is missing. Create an AppPtr to be that previous
      // state, and copy over the mandatory fields. All other fields are the
      // zero "unknown" value.
      apps::mojom::AppPtr state = apps::mojom::App::New();
      state->app_type = delta->app_type;
      state->app_id = delta->app_id;

      using Pair = std::pair<std::string, apps::mojom::AppPtr>;
      iter = states_.insert(Pair(delta->app_id, std::move(state))).first;
    }

    // Merge the delta, updating the internal states_ map. We do this before we
    // notify the observers, so that if an observer calls back into the cache,
    // it will see a consistent snapshot.
    //
    // We also keep (as the clone variable) the states_ value before the merge,
    // so that the observers can know which fields (such as name, icon key or
    // readiness) have changed.
    apps::mojom::AppPtr clone = iter->second.Clone();
    AppUpdate::Merge(iter->second.get(), delta);

    for (auto& obs : observers_) {
      obs.OnAppUpdate(AppUpdate(clone, delta));
    }
  }
}

}  // namespace apps
