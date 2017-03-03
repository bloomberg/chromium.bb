// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_service.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"

namespace doodle {

DoodleService::DoodleService(std::unique_ptr<DoodleFetcher> fetcher,
                             std::unique_ptr<base::OneShotTimer> expiry_timer)
    : fetcher_(std::move(fetcher)), expiry_timer_(std::move(expiry_timer)) {
  DCHECK(fetcher_);
  DCHECK(expiry_timer_);
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
  // Handle the case where the new config is already expired.
  bool expired = time_to_live <= base::TimeDelta();
  const base::Optional<DoodleConfig>& new_config =
      expired ? base::nullopt : doodle_config;

  // If the config changed, update our cache and notify observers.
  // Note that this checks both for existence changes as well as changes of the
  // configs themselves.
  if (cached_config_ != new_config) {
    cached_config_ = new_config;
    for (auto& observer : observers_) {
      observer.OnDoodleConfigUpdated(cached_config_);
    }
  }

  // Even if the configs are identical, the time-to-live might have changed.
  UpdateTimeToLive(time_to_live);
}

void DoodleService::UpdateTimeToLive(base::TimeDelta time_to_live) {
  // (Re-)schedule the cache expiry.
  if (cached_config_.has_value()) {
    expiry_timer_->Start(
        FROM_HERE, time_to_live,
        base::Bind(&DoodleService::DoodleExpired, base::Unretained(this)));
  } else {
    expiry_timer_->Stop();
  }
}

void DoodleService::DoodleExpired() {
  DCHECK(cached_config_.has_value());
  cached_config_.reset();
  for (auto& observer : observers_) {
    observer.OnDoodleConfigUpdated(cached_config_);
  }
}

}  // namespace doodle
