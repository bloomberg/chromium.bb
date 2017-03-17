// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_service.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/doodle/pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace doodle {

// static
void DoodleService::RegisterProfilePrefs(PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterDictionaryPref(prefs::kCachedConfig,
                                        new base::DictionaryValue(),
                                        PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterInt64Pref(prefs::kCachedConfigExpiry, 0,
                                   PrefRegistry::LOSSY_PREF);
}

DoodleService::DoodleService(PrefService* pref_service,
                             std::unique_ptr<DoodleFetcher> fetcher,
                             std::unique_ptr<base::OneShotTimer> expiry_timer,
                             std::unique_ptr<base::Clock> clock)
    : pref_service_(pref_service),
      fetcher_(std::move(fetcher)),
      expiry_timer_(std::move(expiry_timer)),
      clock_(std::move(clock)) {
  DCHECK(pref_service_);
  DCHECK(fetcher_);
  DCHECK(expiry_timer_);
  DCHECK(clock_);

  base::Time expiry_date = base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kCachedConfigExpiry));
  base::Optional<DoodleConfig> config = DoodleConfig::FromDictionary(
      *pref_service_->GetDictionary(prefs::kCachedConfig), base::nullopt);
  UpdateCachedConfig(expiry_date - clock_->Now(), config);
}

DoodleService::~DoodleService() = default;

void DoodleService::Shutdown() {}

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
  UpdateCachedConfig(time_to_live, doodle_config);
}

void DoodleService::UpdateCachedConfig(
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

    if (cached_config_.has_value()) {
      pref_service_->Set(prefs::kCachedConfig, *cached_config_->ToDictionary());
      base::Time expiry_date = clock_->Now() + time_to_live;
      pref_service_->SetInt64(prefs::kCachedConfigExpiry,
                              expiry_date.ToInternalValue());
    } else {
      pref_service_->ClearPref(prefs::kCachedConfig);
      pref_service_->ClearPref(prefs::kCachedConfigExpiry);
    }

    for (auto& observer : observers_) {
      observer.OnDoodleConfigUpdated(cached_config_);
    }
  }

  // Even if the configs are identical, the time-to-live might have changed.
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
  UpdateCachedConfig(base::TimeDelta(), base::nullopt);
}

}  // namespace doodle
