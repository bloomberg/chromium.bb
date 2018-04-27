// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"

#include <algorithm>
#include <vector>

#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {
namespace internal {

namespace {

base::TimeDelta GetTickDeltaSinceEpoch() {
  return resource_coordinator::NowTicks() - base::TimeTicks::UnixEpoch();
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

// static:
const int64_t
    LocalSiteCharacteristicsDataImpl::kZeroIntervalInternalRepresentation =
        LocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
            base::TimeDelta());

void LocalSiteCharacteristicsDataImpl::NotifySiteLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the last loaded time when this origin gets loaded for the first
  // time.
  if (active_webcontents_count_ == 0) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
  }
  active_webcontents_count_++;
}

void LocalSiteCharacteristicsDataImpl::NotifySiteUnloaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_webcontents_count_--;
  // Only update the last loaded time when there's no more loaded instance of
  // this origin.
  if (active_webcontents_count_ > 0U)
    return;

  base::TimeDelta current_unix_time = GetTickDeltaSinceEpoch();
  base::TimeDelta extra_observation_duration =
      current_unix_time -
      InternalRepresentationToTimeDelta(site_characteristics_.last_loaded());

  // Update the |last_loaded_time_| field, as the moment this site gets unloaded
  // also corresponds to the last moment it was loaded.
  site_characteristics_.set_last_loaded(
      TimeDeltaToInternalRepresentation(current_unix_time));

  // Update the observation duration fields.
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    IncrementFeatureObservationDuration(iter, extra_observation_duration);
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::UpdatesFaviconInBackground()
    const {
  return GetFeatureUsage(
      site_characteristics_.updates_favicon_in_background(),
      GetStaticProactiveTabDiscardParams().favicon_update_observation_window);
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::UpdatesTitleInBackground()
    const {
  return GetFeatureUsage(
      site_characteristics_.updates_title_in_background(),
      GetStaticProactiveTabDiscardParams().title_update_observation_window);
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::UsesAudioInBackground()
    const {
  return GetFeatureUsage(
      site_characteristics_.uses_audio_in_background(),
      GetStaticProactiveTabDiscardParams().audio_usage_observation_window);
}

SiteFeatureUsage
LocalSiteCharacteristicsDataImpl::UsesNotificationsInBackground() const {
  return GetFeatureUsage(
      site_characteristics_.uses_notifications_in_background(),
      GetStaticProactiveTabDiscardParams()
          .notifications_usage_observation_window);
}

void LocalSiteCharacteristicsDataImpl::NotifyUpdatesFaviconInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_favicon_in_background());
}

void LocalSiteCharacteristicsDataImpl::NotifyUpdatesTitleInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_title_in_background());
}

void LocalSiteCharacteristicsDataImpl::NotifyUsesAudioInBackground() {
  NotifyFeatureUsage(site_characteristics_.mutable_uses_audio_in_background());
}

void LocalSiteCharacteristicsDataImpl::NotifyUsesNotificationsInBackground() {
  NotifyFeatureUsage(
      site_characteristics_.mutable_uses_notifications_in_background());
}

LocalSiteCharacteristicsDataImpl::LocalSiteCharacteristicsDataImpl(
    const std::string& origin_str,
    OnDestroyDelegate* delegate)
    : origin_str_(origin_str),
      active_webcontents_count_(0U),
      delegate_(delegate) {
  DCHECK_NE(nullptr, delegate_);
  InitWithDefaultValues();
}

base::TimeDelta LocalSiteCharacteristicsDataImpl::FeatureObservationDuration(
    const SiteCharacteristicsFeatureProto& feature_proto) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the current observation duration value, this'll be equal to 0 for
  // features that have been observed.
  base::TimeDelta observation_time_for_feature =
      InternalRepresentationToTimeDelta(feature_proto.observation_duration());

  // If this site is still loaded and the feature isn't in use then the
  // observation time since load needs to be added.
  if (IsLoaded() &&
      InternalRepresentationToTimeDelta(feature_proto.use_timestamp())
          .is_zero()) {
    base::TimeDelta observation_time_since_load =
        GetTickDeltaSinceEpoch() -
        InternalRepresentationToTimeDelta(site_characteristics_.last_loaded());
    observation_time_for_feature += observation_time_since_load;
  }

  return observation_time_for_feature;
}

LocalSiteCharacteristicsDataImpl::~LocalSiteCharacteristicsDataImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's currently required that the site gets unloaded before destroying this
  // object.
  // TODO(sebmarchand): Check if this is a valid assumption.
  DCHECK(!IsLoaded());

  DCHECK_NE(nullptr, delegate_);
  delegate_->OnLocalSiteCharacteristicsDataImplDestroyed(this);
}

// static:
void LocalSiteCharacteristicsDataImpl::IncrementFeatureObservationDuration(
    SiteCharacteristicsFeatureProto* feature_proto,
    base::TimeDelta extra_observation_duration) {
  if (InternalRepresentationToTimeDelta(feature_proto->use_timestamp())
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
  proto->set_observation_duration(kZeroIntervalInternalRepresentation);
  proto->set_use_timestamp(kZeroIntervalInternalRepresentation);
}

void LocalSiteCharacteristicsDataImpl::InitWithDefaultValues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Initialize the feature elements with the default value, this is required
  // because some fields might otherwise never be initialized.
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    InitSiteCharacteristicsFeatureProtoWithDefaultValues(iter);

  site_characteristics_.set_last_loaded(kZeroIntervalInternalRepresentation);
}

void LocalSiteCharacteristicsDataImpl::ClearObservations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reset all the observations.
  InitWithDefaultValues();

  // Set the last loaded time to the current time if there's some loaded
  // instances of this site.
  if (IsLoaded()) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
  }
}

SiteFeatureUsage LocalSiteCharacteristicsDataImpl::GetFeatureUsage(
    const SiteCharacteristicsFeatureProto& feature_proto,
    const base::TimeDelta min_obs_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
    SiteCharacteristicsFeatureProto* feature_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsLoaded());

  feature_proto->set_use_timestamp(
      TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
  feature_proto->set_observation_duration(kZeroIntervalInternalRepresentation);
}

}  // namespace internal
}  // namespace resource_coordinator
