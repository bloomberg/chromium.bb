// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/doodle/pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace doodle {

namespace {

// The maximum time-to-live we'll accept; any larger values will be clamped to
// this one. This is a last resort in case the server sends bad data.
const int64_t kMaxTimeToLiveSecs = 30 * 24 * 60 * 60;  // 30 days

// The default value for DoodleService::min_refresh_interval_.
const int64_t kDefaultMinRefreshIntervalSecs = 15 * 60;  // 15 minutes

}  // namespace

// static
void DoodleService::RegisterProfilePrefs(PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterDictionaryPref(
      prefs::kCachedConfig, base::MakeUnique<base::DictionaryValue>(),
      PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterInt64Pref(prefs::kCachedConfigExpiry, 0,
                                   PrefRegistry::LOSSY_PREF);
}

DoodleService::DoodleService(
    PrefService* pref_service,
    std::unique_ptr<DoodleFetcher> fetcher,
    std::unique_ptr<base::OneShotTimer> expiry_timer,
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<base::TickClock> tick_clock,
    base::Optional<base::TimeDelta> override_min_refresh_interval)
    : pref_service_(pref_service),
      fetcher_(std::move(fetcher)),
      expiry_timer_(std::move(expiry_timer)),
      clock_(std::move(clock)),
      tick_clock_(std::move(tick_clock)),
      min_refresh_interval_(
          override_min_refresh_interval.has_value()
              ? override_min_refresh_interval.value()
              : base::TimeDelta::FromSeconds(kDefaultMinRefreshIntervalSecs)) {
  DCHECK(pref_service_);
  DCHECK(fetcher_);
  DCHECK(expiry_timer_);
  DCHECK(clock_);
  DCHECK(tick_clock_);

  base::Time expiry_date = base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kCachedConfigExpiry));
  base::Optional<DoodleConfig> config = DoodleConfig::FromDictionary(
      *pref_service_->GetDictionary(prefs::kCachedConfig), base::nullopt);
  DoodleState state =
      config.has_value() ? DoodleState::AVAILABLE : DoodleState::NO_DOODLE;
  HandleNewConfig(state, expiry_date - clock_->Now(), config);

  // If we don't have a cached doodle, immediately start a fetch, so that the
  // first NTP will get it quicker.
  if (state == DoodleState::NO_DOODLE) {
    Refresh();
  }
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
  // If we're already waiting for a fetch, don't start another one. The
  // observers will get notified when the ongoing fetch returns.
  if (fetcher_->IsFetchInProgress()) {
    return;
  }

  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  // Check if we have passed the minimum refresh interval.
  base::TimeDelta time_since_fetch = now_ticks - last_successful_fetch_;
  if (time_since_fetch < min_refresh_interval_) {
    RecordDownloadMetrics(OUTCOME_REFRESH_INTERVAL_NOT_PASSED,
                          base::TimeDelta());
    for (auto& observer : observers_) {
      observer.OnDoodleConfigRevalidated(/*from_cache=*/true);
    }
    return;
  }

  fetcher_->FetchDoodle(base::BindOnce(&DoodleService::DoodleFetched,
                                       base::Unretained(this), now_ticks));
}

// static
bool DoodleService::DownloadOutcomeIsSuccess(DownloadOutcome outcome) {
  switch (outcome) {
    case OUTCOME_NEW_DOODLE:
    case OUTCOME_REVALIDATED_DOODLE:
    case OUTCOME_CHANGED_DOODLE:
    case OUTCOME_NO_DOODLE:
      return true;
    case OUTCOME_EXPIRED:
    case OUTCOME_DOWNLOAD_ERROR:
    case OUTCOME_PARSING_ERROR:
    case OUTCOME_REFRESH_INTERVAL_NOT_PASSED:
      return false;
    case OUTCOME_COUNT:
      NOTREACHED();
  }
  return false;
}

// static
void DoodleService::RecordDownloadMetrics(DownloadOutcome outcome,
                                          base::TimeDelta download_time) {
  UMA_HISTOGRAM_ENUMERATION("Doodle.ConfigDownloadOutcome", outcome,
                            OUTCOME_COUNT);

  if (DownloadOutcomeIsSuccess(outcome)) {
    UMA_HISTOGRAM_MEDIUM_TIMES("Doodle.ConfigDownloadTime", download_time);
  }
}

// static
DoodleService::DownloadOutcome DoodleService::DetermineDownloadOutcome(
    const base::Optional<DoodleConfig>& old_config,
    const base::Optional<DoodleConfig>& new_config,
    DoodleState state,
    bool expired) {
  // First, handle error cases: *_ERROR or EXPIRED override other outcomes.
  switch (state) {
    case DoodleState::AVAILABLE:
      if (expired) {
        return OUTCOME_EXPIRED;
      }
      break;

    case DoodleState::NO_DOODLE:
      break;

    case DoodleState::DOWNLOAD_ERROR:
      return OUTCOME_DOWNLOAD_ERROR;

    case DoodleState::PARSING_ERROR:
      return OUTCOME_PARSING_ERROR;
  }

  if (!new_config.has_value()) {
    return OUTCOME_NO_DOODLE;
  }
  if (!old_config.has_value()) {
    return OUTCOME_NEW_DOODLE;
  }
  if (old_config.value() != new_config.value()) {
    return OUTCOME_CHANGED_DOODLE;
  }
  return OUTCOME_REVALIDATED_DOODLE;
}

void DoodleService::DoodleFetched(
    base::TimeTicks start_time,
    DoodleState state,
    base::TimeDelta time_to_live,
    const base::Optional<DoodleConfig>& doodle_config) {
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  DownloadOutcome outcome = HandleNewConfig(state, time_to_live, doodle_config);
  if (DownloadOutcomeIsSuccess(outcome)) {
    last_successful_fetch_ = now_ticks;
  }
  base::TimeDelta download_time = now_ticks - start_time;
  RecordDownloadMetrics(outcome, download_time);
}

DoodleService::DownloadOutcome DoodleService::HandleNewConfig(
    DoodleState state,
    base::TimeDelta time_to_live,
    const base::Optional<DoodleConfig>& doodle_config) {
  // Clamp the time-to-live to some reasonable maximum.
  if (time_to_live.InSeconds() > kMaxTimeToLiveSecs) {
    time_to_live = base::TimeDelta::FromSeconds(kMaxTimeToLiveSecs);
    DLOG(WARNING) << "Clamping TTL to " << kMaxTimeToLiveSecs << " seconds!";
  }

  // Handle the case where the new config is already expired.
  bool expired = time_to_live <= base::TimeDelta();
  const base::Optional<DoodleConfig>& new_config =
      expired ? base::nullopt : doodle_config;

  // Determine the download outcome *before* updating the cached config.
  DownloadOutcome outcome =
      DetermineDownloadOutcome(cached_config_, new_config, state, expired);

  // Note that this checks both for existence changes as well as changes of the
  // configs themselves.
  if (cached_config_ != new_config) {
    UpdateCachedConfig(time_to_live, new_config);

    for (auto& observer : observers_) {
      observer.OnDoodleConfigUpdated(cached_config_);
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnDoodleConfigRevalidated(/*from_cache=*/false);
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

  return outcome;
}

void DoodleService::UpdateCachedConfig(
    base::TimeDelta time_to_live,
    const base::Optional<DoodleConfig>& new_config) {
  DCHECK(cached_config_ != new_config);

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
}

void DoodleService::DoodleExpired() {
  DCHECK(cached_config_.has_value());
  HandleNewConfig(DoodleState::NO_DOODLE, base::TimeDelta(), base::nullopt);
}

}  // namespace doodle
