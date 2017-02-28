// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOODLE_DOODLE_SERVICE_H_
#define COMPONENTS_DOODLE_DOODLE_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/doodle/doodle_fetcher.h"
#include "components/doodle/doodle_types.h"

namespace doodle {

class DoodleService {
 public:
  class Observer {
   public:
    virtual void OnDoodleConfigUpdated(const base::Optional<DoodleConfig>&) = 0;
  };

  DoodleService(std::unique_ptr<DoodleFetcher> fetcher);
  ~DoodleService();

  // Returns the current (cached) config, if any.
  const base::Optional<DoodleConfig>& config() const { return cached_config_; }

  // Adds a new observer to the service. It'll only be called when the config
  // changes; to get the current (cached) config, call |config()|.
  void AddObserver(Observer* observer);

  // Prevents |observer| from receiving future updates. This is safe to call
  // even when the observer is being notified of an update.
  void RemoveObserver(Observer* observer);

  // Requests an asynchronous refresh of the DoodleConfig from the network.
  // After the update completes, the observers will be notified only if the
  // config changed.
  void Refresh();

 private:
  void DoodleFetched(DoodleState state,
                     const base::Optional<DoodleConfig>& doodle_config);

  // The fetcher for getting fresh DoodleConfigs from the network.
  std::unique_ptr<DoodleFetcher> fetcher_;

  // The result of the last network fetch.
  base::Optional<DoodleConfig> cached_config_;

  // The list of observers to be notified when the DoodleConfig changes.
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(DoodleService);
};

}  // namespace doodle

#endif  // COMPONENTS_DOODLE_DOODLE_SERVICE_H_
