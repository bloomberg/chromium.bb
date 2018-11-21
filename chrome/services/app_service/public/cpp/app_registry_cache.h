// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/observer_list_types.h"
#include "chrome/services/app_service/public/cpp/app_update.h"

namespace apps {

// Caches all of the apps::mojom::AppPtr's seen by an apps::mojom::Subscriber.
// A Subscriber sees a stream of "deltas", or changes in app state. This cache
// also keeps the "sum" of those previous deltas, so that observers of this
// object are presented with AppUpdate's, i.e. "state-and-delta"s.
//
// It can also be queried synchronously, providing answers from its in-memory
// cache, even though the underlying App Registry (and its App Publishers)
// communicate asynchronously, possibly across process boundaries, via Mojo
// IPC. Synchronous APIs can be more suitable for e.g. UI programming that
// should not block an event loop on I/O.
//
// This class is not thread-safe.
//
// See //chrome/services/app_service/README.md for more details.
class AppRegistryCache {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}

    // The apps::AppUpdate argument shouldn't be accessed after OnAppUpdate
    // returns.
    virtual void OnAppUpdate(const AppUpdate& update) = 0;
  };

  AppRegistryCache();
  ~AppRegistryCache();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies all observers of state-and-delta AppUpdate's (the state comes
  // from the internal cache, the delta comes from the argument) and then
  // merges the cached states with the deltas.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas);

  // Calls f, a void-returning function whose arguments are (const
  // apps::AppUpdate&), on each app in the cache. The AppUpdate (a
  // state-and-delta) is equivalent to the delta being "all unknown" or "no
  // changes" and the state being the "sum" of all previous deltas. None of the
  // AppUpdate's FooChanged methods should return true.
  //
  // f's argument is an apps::AppUpdate instead of an apps::mojom::AppPtr so
  // that callers can more easily share code with Observer::OnAppUpdate (which
  // also takes an apps::AppUpdate).
  //
  // The apps::AppUpdate argument to f shouldn't be accessed after f returns.
  template <typename FunctionType>
  void ForEachApp(FunctionType f) {
    for (const auto& iter : states_) {
      f(apps::AppUpdate(iter.second, iter.second));
    }
  }

 private:
  base::ObserverList<Observer> observers_;

  // Maps from app_id to the latest state: the "sum" of all previous deltas.
  std::map<std::string, apps::mojom::AppPtr> states_;

  DISALLOW_COPY_AND_ASSIGN(AppRegistryCache);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_
