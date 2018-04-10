// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_IMPL_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/site_characteristics.pb.h"

namespace resource_coordinator {
namespace internal {

// A tri-state return value for site feature usage. If a definitive decision
// can't be made then an "unknown" result can be returned.
enum class SiteFeatureUsage {
  SITE_FEATURE_NOT_IN_USE,
  SITE_FEATURE_IN_USE,
  SITE_FEATURE_USAGE_UNKNOWN,
};

// Tracks observations for a given site. This class shouldn't be used
// directly, it's meant to be used internally by the local site heuristic
// database.
class LocalSiteCharacteristicsDataImpl
    : public base::RefCounted<LocalSiteCharacteristicsDataImpl> {
 public:
  explicit LocalSiteCharacteristicsDataImpl(const std::string& origin_str);

  // Must be called when a load event is received for this site, this can be
  // invoked several time if instances of this class are shared between
  // multiple tabs.
  void NotifySiteLoaded();

  // Must be called when an unload event is received for this site, this can be
  // invoked several time if instances of this class are shared between
  // multiple tabs.
  void NotifySiteUnloaded();

  // Returns the usage of a given feature for this origin.
  SiteFeatureUsage UpdatesFaviconInBackground() const;
  SiteFeatureUsage UpdatesTitleInBackground() const;
  SiteFeatureUsage UsesAudioInBackground() const;
  SiteFeatureUsage UsesNotificationsInBackground() const;

  // Must be called when a feature is used, calling this function updates the
  // last observed timestamp for this feature.
  void NotifyUpdatesFaviconInBackground();
  void NotifyUpdatesTitleInBackground();
  void NotifyUsesAudioInBackground();
  void NotifyUsesNotificationsInBackground();

  // TODO(sebmarchand): Add the methods necessary to record other types of
  // observations (e.g. memory and CPU usage).

 protected:
  friend class base::RefCounted<LocalSiteCharacteristicsDataImpl>;
  friend class LocalSiteCharacteristicsDataImplTest;

  // Helper functions to convert from/to the internal representation that is
  // used to store TimeDelta values in the |SiteCharacteristicsProto| protobuf.
  static base::TimeDelta InternalRepresentationToTimeDelta(
      ::google::protobuf::int64 value) {
    return base::TimeDelta::FromSeconds(value);
  }
  static int64_t TimeDeltaToInternalRepresentation(base::TimeDelta delta) {
    return delta.InSeconds();
  }

  // Returns the minimum observation time before considering a feature as
  // unused.
  static constexpr base::TimeDelta
  GetUpdatesFaviconInBackgroundMinObservationWindow() {
    return base::TimeDelta::FromHours(2);
  }
  static constexpr base::TimeDelta
  GetUpdatesTitleInBackgroundMinObservationWindow() {
    return base::TimeDelta::FromHours(2);
  }
  static constexpr base::TimeDelta
  GetUsesAudioInBackgroundMinObservationWindow() {
    return base::TimeDelta::FromHours(2);
  }
  static constexpr base::TimeDelta
  GetUsesNotificationsInBackgroundMinObservationWindow() {
    return base::TimeDelta::FromHours(2);
  }

  virtual ~LocalSiteCharacteristicsDataImpl();

  // Returns the observation duration for a given feature, this is the sum of
  // the recorded observation duration and the current observation duration
  // since this site has been loaded (if applicable). If a feature has been
  // used then it returns 0.
  base::TimeDelta FeatureObservationDuration(
      const SiteCharacteristicsFeatureProto& feature_proto) const;

  // Accessors, for testing:
  base::TimeDelta last_loaded_time_for_testing() const {
    return InternalRepresentationToTimeDelta(
        site_characteristics_.last_loaded());
  }

  const SiteCharacteristicsProto& site_characteristics_for_testing() const {
    return site_characteristics_;
  }

  static const int64_t kZeroIntervalInternalRepresentation;

 private:
  // Add |extra_observation_duration| to the observation window of a given
  // feature if it hasn't been used yet, do nothing otherwise.
  static void IncrementFeatureObservationDuration(
      SiteCharacteristicsFeatureProto* feature_proto,
      base::TimeDelta extra_observation_duration);

  // Initialize a SiteCharacteristicsFeatureProto object with its default
  // values.
  static void InitSiteCharacteristicsFeatureProtoWithDefaultValues(
      SiteCharacteristicsFeatureProto* proto);

  // Returns the usage of |site_feature| for this site.
  SiteFeatureUsage GetFeatureUsage(
      const SiteCharacteristicsFeatureProto& feature_proto,
      const base::TimeDelta min_obs_time) const;

  // Helper function to update a given |SiteCharacteristicsFeatureProto| when a
  // feature gets used.
  void NotifyFeatureUsage(SiteCharacteristicsFeatureProto* feature_proto);

  // This site's characteristics, contains the features and other values are
  // measured.
  SiteCharacteristicsProto site_characteristics_;

  // This site's origin.
  const std::string origin_str_;

  // The number of active WebContents for this origin. Several tabs with the
  // same origin might share the same instance of this object, this counter
  // will allow to properly update the observation time (starts when the first
  // tab gets loaded, stops when the last one gets unloaded).
  size_t active_webcontents_count_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataImpl);
};

}  // namespace internal
}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_IMPL_H_
