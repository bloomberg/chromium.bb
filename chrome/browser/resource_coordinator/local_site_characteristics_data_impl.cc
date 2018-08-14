// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_database.h"
#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {
namespace internal {

namespace {

// The sample weighing factor for the exponential moving averages for
// performance measurements. A factor of 1/2 gives each sample an equal weight
// to the entire previous history. As we don't know much noise there is to the
// measurement, this is essentially a shot in the dark.
// TODO(siggi): Consider adding UMA metrics to capture e.g. the fractional delta
//      from the current average, or some such.
constexpr float kSampleWeightFactor = 0.5;

base::TimeDelta GetTickDeltaSinceEpoch() {
  return NowTicks() - base::TimeTicks::UnixEpoch();
}

// Returns all the SiteCharacteristicsFeatureProto elements contained in a
// SiteCharacteristicsProto protobuf object.
std::vector<SiteCharacteristicsFeatureProto*> GetAllFeaturesFromProto(
    SiteCharacteristicsProto* proto) {
  std::vector<SiteCharacteristicsFeatureProto*> ret(
      {proto->mutable_updates_favicon_in_background(),
       proto->mutable_updates_title_in_background(),
       proto->mutable_uses_audio_in_background(),
       proto->mutable_uses_notifications_in_background()});

  return ret;
}

}  // namespace

void LocalSiteCharacteristicsDataImpl::NotifySiteLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the last loaded time when this origin gets loaded for the first
  // time.
  if (loaded_tabs_count_ == 0) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));

    is_dirty_ = true;
  }
  loaded_tabs_count_++;
}

void LocalSiteCharacteristicsDataImpl::NotifySiteUnloaded(
    TabVisibility tab_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (tab_visibility == TabVisibility::kBackground)
    DecrementNumLoadedBackgroundTabs();

  loaded_tabs_count_--;
  // Only update the last loaded time when there's no more loaded instance of
  // this origin.
  if (loaded_tabs_count_ > 0U)
    return;

  base::TimeDelta current_unix_time = GetTickDeltaSinceEpoch();

  // Update the |last_loaded_time_| field, as the moment this site gets unloaded
  // also corresponds to the last moment it was loaded.
  site_characteristics_.set_last_loaded(
      TimeDeltaToInternalRepresentation(current_unix_time));
}

void LocalSiteCharacteristicsDataImpl::NotifyLoadedSiteBackgrounded() {
  if (loaded_tabs_in_background_count_ == 0)
    background_session_begin_ = NowTicks();

  loaded_tabs_in_background_count_++;

  DCHECK_LE(loaded_tabs_in_background_count_, loaded_tabs_count_);
}

void LocalSiteCharacteristicsDataImpl::NotifyLoadedSiteForegrounded() {
  DecrementNumLoadedBackgroundTabs();
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::UpdatesFaviconInBackground()
    const {
  return GetFeatureUsage(
      site_characteristics_.updates_favicon_in_background(),
      GetSiteCharacteristicsDatabaseParams().favicon_update_observation_window);
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::UpdatesTitleInBackground()
    const {
  return GetFeatureUsage(
      site_characteristics_.updates_title_in_background(),
      GetSiteCharacteristicsDatabaseParams().title_update_observation_window);
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::UsesAudioInBackground()
    const {
  return GetFeatureUsage(
      site_characteristics_.uses_audio_in_background(),
      GetSiteCharacteristicsDatabaseParams().audio_usage_observation_window);
}

SiteFeatureUsage
LocalSiteCharacteristicsDataImpl::UsesNotificationsInBackground() const {
  return GetFeatureUsage(
      site_characteristics_.uses_notifications_in_background(),
      GetSiteCharacteristicsDatabaseParams()
          .notifications_usage_observation_window);
}

void LocalSiteCharacteristicsDataImpl::NotifyUpdatesFaviconInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_favicon_in_background(),
      "FaviconUpdateInBackground");
}

void LocalSiteCharacteristicsDataImpl::NotifyUpdatesTitleInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_title_in_background(),
      "TitleUpdateInBackground");
}

void LocalSiteCharacteristicsDataImpl::NotifyUsesAudioInBackground() {
  NotifyFeatureUsage(site_characteristics_.mutable_uses_audio_in_background(),
                     "AudioUsageInBackground");
}

void LocalSiteCharacteristicsDataImpl::NotifyUsesNotificationsInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_uses_notifications_in_background(),
      "NotificationsUsageInBackground");
}

void LocalSiteCharacteristicsDataImpl::NotifyLoadTimePerformanceMeasurement(
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {
  is_dirty_ = true;

  cpu_usage_estimate_.AppendDatum(cpu_usage_estimate.InMicroseconds());
  private_footprint_kb_estimate_.AppendDatum(private_footprint_kb_estimate);
}

void LocalSiteCharacteristicsDataImpl::ExpireAllObservationWindowsForTesting() {
  auto params = GetSiteCharacteristicsDatabaseParams();
  base::TimeDelta longest_observation_window =
      std::max({params.favicon_update_observation_window,
                params.title_update_observation_window,
                params.audio_usage_observation_window,
                params.notifications_usage_observation_window});
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    IncrementFeatureObservationDuration(iter, longest_observation_window);
}

LocalSiteCharacteristicsDataImpl::LocalSiteCharacteristicsDataImpl(
    const url::Origin& origin,
    OnDestroyDelegate* delegate,
    LocalSiteCharacteristicsDatabase* database)
    : cpu_usage_estimate_(kSampleWeightFactor),
      private_footprint_kb_estimate_(kSampleWeightFactor),
      origin_(origin),
      loaded_tabs_count_(0U),
      loaded_tabs_in_background_count_(0U),
      database_(database),
      delegate_(delegate),
      fully_initialized_(false),
      is_dirty_(false),
      weak_factory_(this) {
  DCHECK(database_);
  DCHECK(delegate_);
  DCHECK(!site_characteristics_.IsInitialized());

  database_->ReadSiteCharacteristicsFromDB(
      origin_, base::BindOnce(&LocalSiteCharacteristicsDataImpl::OnInitCallback,
                              weak_factory_.GetWeakPtr()));
}

LocalSiteCharacteristicsDataImpl::~LocalSiteCharacteristicsDataImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // All users of this object should make sure that they send the same number of
  // NotifySiteLoaded and NotifySiteUnloaded events, in practice this mean
  // tracking the loaded state and sending an unload event in their destructor
  // if needed.
  DCHECK(!IsLoaded());
  DCHECK_EQ(0U, loaded_tabs_in_background_count_);

  DCHECK(delegate_);
  delegate_->OnLocalSiteCharacteristicsDataImplDestroyed(this);

  // TODO(sebmarchand): Some data might be lost here if the read operation has
  // not completed, add some metrics to measure if this is really an issue.
  if (is_dirty_ && fully_initialized_) {
    DCHECK(site_characteristics_.IsInitialized());

    // Update the proto with the most current performance measurement averages.
    if (cpu_usage_estimate_.num_datums() ||
        private_footprint_kb_estimate_.num_datums()) {
      auto* estimates = site_characteristics_.mutable_load_time_estimates();
      if (cpu_usage_estimate_.num_datums())
        estimates->set_avg_cpu_usage_us(cpu_usage_estimate_.value());
      if (private_footprint_kb_estimate_.num_datums()) {
        estimates->set_avg_footprint_kb(private_footprint_kb_estimate_.value());
      }
    }
    database_->WriteSiteCharacteristicsIntoDB(origin_, site_characteristics_);
  }
}

base::TimeDelta LocalSiteCharacteristicsDataImpl::FeatureObservationDuration(
    const SiteCharacteristicsFeatureProto& feature_proto) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the current observation duration value, this'll be equal to 0 for
  // features that have been observed.
  base::TimeDelta observation_time_for_feature =
      InternalRepresentationToTimeDelta(feature_proto.observation_duration());

  // If this site is still in background and the feature isn't in use then the
  // observation time since load needs to be added.
  if (loaded_tabs_in_background_count_ > 0U &&
      InternalRepresentationToTimeDelta(feature_proto.use_timestamp())
          .is_zero()) {
    base::TimeDelta observation_time_since_backgrounded =
        NowTicks() - background_session_begin_;
    observation_time_for_feature += observation_time_since_backgrounded;
  }

  return observation_time_for_feature;
}

// static:
void LocalSiteCharacteristicsDataImpl::IncrementFeatureObservationDuration(
    SiteCharacteristicsFeatureProto* feature_proto,
    base::TimeDelta extra_observation_duration) {
  if (!feature_proto->has_use_timestamp() ||
      InternalRepresentationToTimeDelta(feature_proto->use_timestamp())
          .is_zero()) {
    feature_proto->set_observation_duration(TimeDeltaToInternalRepresentation(
        InternalRepresentationToTimeDelta(
            feature_proto->observation_duration()) +
        extra_observation_duration));
  }
}

// static:
void LocalSiteCharacteristicsDataImpl::
    InitSiteCharacteristicsFeatureProtoWithDefaultValues(
        SiteCharacteristicsFeatureProto* proto) {
  DCHECK_NE(nullptr, proto);
  static const auto zero_interval =
      LocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          base::TimeDelta());
  proto->set_observation_duration(zero_interval);
  proto->set_use_timestamp(zero_interval);
}

void LocalSiteCharacteristicsDataImpl::InitWithDefaultValues(
    bool only_init_uninitialized_fields) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Initialize the feature elements with the default value, this is required
  // because some fields might otherwise never be initialized.
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_)) {
    if (!only_init_uninitialized_fields || !iter->IsInitialized())
      InitSiteCharacteristicsFeatureProtoWithDefaultValues(iter);
  }

  if (!only_init_uninitialized_fields ||
      !site_characteristics_.has_last_loaded()) {
    site_characteristics_.set_last_loaded(
        LocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
            base::TimeDelta()));
  }
}

void LocalSiteCharacteristicsDataImpl::
    ClearObservationsAndInvalidateReadOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate the weak pointer that have been served, this will ensure that
  // this object doesn't get initialized from the database after being cleared.
  weak_factory_.InvalidateWeakPtrs();

  // Reset all the observations.
  InitWithDefaultValues(false);

  // Clear the performance estimates, both the local state and the proto.
  cpu_usage_estimate_.Clear();
  private_footprint_kb_estimate_.Clear();
  site_characteristics_.clear_load_time_estimates();

  // Set the last loaded time to the current time if there's some loaded
  // instances of this site.
  if (IsLoaded()) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
  }

  // This object is now in a valid state and can be written in the database.
  fully_initialized_ = true;
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::GetFeatureUsage(
    const SiteCharacteristicsFeatureProto& feature_proto,
    const base::TimeDelta min_obs_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_BOOLEAN(
      "ResourceCoordinator.LocalDB.ReadHasCompletedBeforeQuery",
      fully_initialized_);

  if (!feature_proto.IsInitialized())
    return SiteFeatureUsage::kSiteFeatureUsageUnknown;

  // Checks if this feature has already been observed.
  // TODO(sebmarchand): Check the timestamp and reset features that haven't been
  // observed in a long time, https://crbug.com/826446.
  if (!InternalRepresentationToTimeDelta(feature_proto.use_timestamp())
           .is_zero()) {
    return SiteFeatureUsage::kSiteFeatureInUse;
  }

  if (FeatureObservationDuration(feature_proto) >= min_obs_time)
    return SiteFeatureUsage::kSiteFeatureNotInUse;

  return SiteFeatureUsage::kSiteFeatureUsageUnknown;
}

void LocalSiteCharacteristicsDataImpl::NotifyFeatureUsage(
    SiteCharacteristicsFeatureProto* feature_proto,
    const char* feature_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsLoaded());
  DCHECK_GT(loaded_tabs_in_background_count_, 0U);

  // Report the observation time if this is the first time this feature is
  // observed.
  if (feature_proto->observation_duration() != 0) {
    base::UmaHistogramCustomTimes(
        base::StringPrintf(
            "ResourceCoordinator.LocalDB.ObservationTimeBeforeFirstUse.%s",
            feature_name),
        InternalRepresentationToTimeDelta(
            feature_proto->observation_duration()),
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(1), 100);
  }

  feature_proto->set_use_timestamp(
      TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
  feature_proto->set_observation_duration(
      LocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          base::TimeDelta()));
}

void LocalSiteCharacteristicsDataImpl::OnInitCallback(
    base::Optional<SiteCharacteristicsProto> db_site_characteristics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check if the initialization has succeeded.
  if (db_site_characteristics) {
    // If so, iterates over all the features and initialize them.
    auto this_features = GetAllFeaturesFromProto(&site_characteristics_);
    auto db_features =
        GetAllFeaturesFromProto(&db_site_characteristics.value());
    auto this_features_iter = this_features.begin();
    auto db_features_iter = db_features.begin();
    for (; this_features_iter != this_features.end() &&
           db_features_iter != db_features.end();
         ++this_features_iter, ++db_features_iter) {
      // If the |use_timestamp| field is set for the in-memory entry for this
      // feature then there's nothing to do, otherwise update it with the values
      // from the database.
      if (!(*this_features_iter)->has_use_timestamp()) {
        if ((*db_features_iter)->has_use_timestamp() &&
            (*db_features_iter)->use_timestamp() != 0) {
          // Keep the use timestamp from the database, if any.
          (*this_features_iter)
              ->set_use_timestamp((*db_features_iter)->use_timestamp());
          (*this_features_iter)
              ->set_observation_duration(
                  LocalSiteCharacteristicsDataImpl::
                      TimeDeltaToInternalRepresentation(base::TimeDelta()));
        } else {
          // Else, add the observation duration from the database to the
          // in-memory observation duration.
          if (!(*this_features_iter)->has_observation_duration()) {
            (*this_features_iter)
                ->set_observation_duration(
                    LocalSiteCharacteristicsDataImpl::
                        TimeDeltaToInternalRepresentation(base::TimeDelta()));
          }
          IncrementFeatureObservationDuration(
              (*this_features_iter),
              InternalRepresentationToTimeDelta(
                  (*db_features_iter)->observation_duration()));
          // Makes sure that the |use_timestamp| field gets initialized.
          (*this_features_iter)
              ->set_use_timestamp(
                  LocalSiteCharacteristicsDataImpl::
                      TimeDeltaToInternalRepresentation(base::TimeDelta()));
        }
      }
    }
    // Only update the last loaded field if we haven't updated it since the
    // creation of this object.
    if (!site_characteristics_.has_last_loaded()) {
      site_characteristics_.set_last_loaded(
          db_site_characteristics->last_loaded());
    }

    // If there was on-disk data, update the in-memory performance averages.
    if (db_site_characteristics->has_load_time_estimates()) {
      const auto& estimates = db_site_characteristics->load_time_estimates();
      if (estimates.has_avg_cpu_usage_us())
        cpu_usage_estimate_.PrependDatum(estimates.avg_cpu_usage_us());
      if (estimates.has_avg_footprint_kb()) {
        private_footprint_kb_estimate_.PrependDatum(
            estimates.avg_footprint_kb());
      }
    }
  } else {
    // Init all the fields that haven't been initialized with a default value.
    InitWithDefaultValues(true /* only_init_uninitialized_fields */);
  }

  fully_initialized_ = true;
  DCHECK(site_characteristics_.IsInitialized());
}

void LocalSiteCharacteristicsDataImpl::DecrementNumLoadedBackgroundTabs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(loaded_tabs_in_background_count_, 0U);
  loaded_tabs_in_background_count_--;
  // Only update the observation durations if there's no more backgounded
  // instance of this origin.
  if (loaded_tabs_in_background_count_ > 0U)
    return;

  DCHECK(!background_session_begin_.is_null());
  base::TimeDelta extra_observation_duration =
      NowTicks() - background_session_begin_;

  // Update the observation duration fields.
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    IncrementFeatureObservationDuration(iter, extra_observation_duration);
}

}  // namespace internal
}  // namespace resource_coordinator
