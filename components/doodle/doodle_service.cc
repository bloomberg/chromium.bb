// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_service.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"

namespace doodle {

DoodleService::DoodleService(std::unique_ptr<DoodleFetcher> fetcher)
    : fetcher_(std::move(fetcher)) {
  DCHECK(fetcher_);
}

DoodleService::~DoodleService() = default;

void DoodleService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DoodleService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DoodleService::Refresh() {
  fetcher_->FetchDoodle(
      base::BindOnce(&DoodleService::DoodleFetched, base::Unretained(this)));
}

void DoodleService::DoodleFetched(
    DoodleState state,
    base::TimeDelta time_to_live,
    const base::Optional<DoodleConfig>& doodle_config) {
  // If nothing changed, then there's nothing to do. Note that this checks both
  // for existence changes as well as changes of the configs themselves.
  if (cached_config_ == doodle_config) {
    // TODO(treib): Once we support expiry, update the time to live here.
    return;
  }

  // Update the cache and notify observers.
  cached_config_ = doodle_config;
  for (auto& observer : observers_) {
    observer.OnDoodleConfigUpdated(cached_config_);
  }
}

}  // namespace doodle
