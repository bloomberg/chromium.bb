// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_service.h"

#include <utility>

#include "base/bind.h"

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
    const base::Optional<DoodleConfig>& doodle_config) {
  if (!cached_config_.has_value() && !doodle_config.has_value()) {
    // There was no config before and we didn't get a new one, so there's
    // nothing to do.
    return;
  }

  bool notify = false;
  if (cached_config_.has_value() != doodle_config.has_value()) {
    // We got a new config, or an existing one went away.
    notify = true;
  } else {
    // There was a config both before and after the update. Notify observers
    // only if something relevant changed.
    notify = !cached_config_.value().IsEquivalent(doodle_config.value());
  }

  // In any case, update the cache.
  cached_config_ = doodle_config;

  if (notify) {
    for (auto& observer : observers_) {
      observer.OnDoodleConfigUpdated(cached_config_);
    }
  }
}

}  // namespace doodle
