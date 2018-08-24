// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_IMPL_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/exponential_moving_average.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_database.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_feature_usage.h"
#include "chrome/browser/resource_coordinator/site_characteristics.pb.h"
#include "chrome/browser/resource_coordinator/site_characteristics_tab_visibility.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "url/origin.h"

namespace resource_coordinator {

class LocalSiteCharacteristicsDatabase;
class LocalSiteCharacteristicsDataStore;
class LocalSiteCharacteristicsDataStoreTest;
class LocalSiteCharacteristicsDataReaderTest;
class LocalSiteCharacteristicsDataWriterTest;

FORWARD_DECLARE_TEST(LocalSiteCharacteristicsDataReaderTest,
                     FreeingReaderDoesntCauseWriteOperation);

namespace internal {

FORWARD_DECLARE_TEST(LocalSiteCharacteristicsDataImplTest,
                     LateAsyncReadDoesntBypassClearEvent);

// Internal class used to read/write site characteristics. This is a wrapper
// class around a SiteCharacteristicsProto object and offers various to query
// and/or modify it. This class shouldn't be used directly, instead it should be
// created by a LocalSiteCharacteristicsDataStore that will serve reader and
// writer objects.
//
// Reader and writers objects that are interested in reading/writing information
// about the same origin will share a unique ref counted instance of this
// object, because of this all the operations done on these objects should be
// done on the same thread, this class isn't thread safe.
//
// By default tabs associated with instances of this class are assumed to be
// running in foreground, |NotifyTabBackgrounded| should get called to indicate
// that the tab is running in background.
class LocalSiteCharacteristicsDataImpl
    : public base::RefCounted<LocalSiteCharacteristicsDataImpl> {
 public:
  // Interface that should be implemented in order to receive notifications when
  // this object is about to get destroyed.
  class OnDestroyDelegate {
   public:
    // Called when this object is about to get destroyed.
    virtual void OnLocalSiteCharacteristicsDataImplDestroyed(
        LocalSiteCharacteristicsDataImpl* impl) = 0;
  };

  // Must be called when a load event is received for this site, this can be
  // invoked several times if instances of this class are shared between
  // multiple tabs.
  void NotifySiteLoaded();

  // Must be called when an unload event is received for this site, this can be
  // invoked several times if instances of this class are shared between
  // multiple tabs.
  void NotifySiteUnloaded(TabVisibility tab_visibility);

  // Must be called when a loaded tab gets backgrounded.
  void NotifyLoadedSiteBackgrounded();

  // Must be called when a loaded tab gets foregrounded.
  void NotifyLoadedSiteForegrounded();

  // Returns the usage of a given feature for this origin.
  SiteFeatureUsage UpdatesFaviconInBackground() const;
  SiteFeatureUsage UpdatesTitleInBackground() const;
  SiteFeatureUsage UsesAudioInBackground() const;
  SiteFeatureUsage UsesNotificationsInBackground() const;

  // Accessors for load-time performance measurement estimates.
  // If |num_datum| is zero, there's no estimate available.
  const ExponentialMovingAverage& cpu_usage_estimate() const {
    return cpu_usage_estimate_;
  }
  const ExponentialMovingAverage& private_footprint_kb_estimate() const {
    return private_footprint_kb_estimate_;
  }

  // Must be called when a feature is used, calling this function updates the
  // last observed timestamp for this feature.
  void NotifyUpdatesFaviconInBackground();
  void NotifyUpdatesTitleInBackground();
  void NotifyUsesAudioInBackground();
  void NotifyUsesNotificationsInBackground();

  // Call when a load-time performance measurement becomes available.
  void NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate);

  base::TimeDelta last_loaded_time_for_testing() const {
    return InternalRepresentationToTimeDelta(
        site_characteristics_.last_loaded());
  }

  const SiteCharacteristicsProto& site_characteristics_for_testing() const {
    return site_characteristics_;
  }

  size_t loaded_tabs_count_for_testing() const { return loaded_tabs_count_; }

  size_t loaded_tabs_in_background_count_for_testing() const {
    return loaded_tabs_in_background_count_;
  }

  base::TimeTicks background_session_begin_for_testing() const {
    return background_session_begin_;
  }

  const url::Origin& origin() const { return origin_; }

  void ExpireAllObservationWindowsForTesting();

  void ClearObservationsAndInvalidateReadOperationForTesting() {
    ClearObservationsAndInvalidateReadOperation();
  }

 protected:
  friend class base::RefCounted<LocalSiteCharacteristicsDataImpl>;
  friend class resource_coordinator::LocalSiteCharacteristicsDataStore;

  // Friend all the tests.
  friend class LocalSiteCharacteristicsDataImplTest;
  friend class resource_coordinator::LocalSiteCharacteristicsDataReaderTest;
  friend class resource_coordinator::LocalSiteCharacteristicsDataStoreTest;
  friend class resource_coordinator::LocalSiteCharacteristicsDataWriterTest;

  LocalSiteCharacteristicsDataImpl(const url::Origin& origin,
                                   OnDestroyDelegate* delegate,
                                   LocalSiteCharacteristicsDatabase* database);

  virtual ~LocalSiteCharacteristicsDataImpl();

  // Helper functions to convert from/to the internal representation that is
  // used to store TimeDelta values in the |SiteCharacteristicsProto| protobuf.
  static base::TimeDelta InternalRepresentationToTimeDelta(
      ::google::protobuf::int64 value) {
    return base::TimeDelta::FromSeconds(value);
  }
  static int64_t TimeDeltaToInternalRepresentation(base::TimeDelta delta) {
    return delta.InSeconds();
  }

  // Returns for how long a given feature has been observed, this is the sum of
  // the recorded observation duration and the current observation duration
  // since this site has been loaded (if applicable). If a feature has been
  // used then it returns 0.
  base::TimeDelta FeatureObservationDuration(
      const SiteCharacteristicsFeatureProto& feature_proto) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataImplTest,
                           LateAsyncReadDoesntBypassClearEvent);
  FRIEND_TEST_ALL_PREFIXES(
      resource_coordinator::LocalSiteCharacteristicsDataReaderTest,
      FreeingReaderDoesntCauseWriteOperation);

  // Add |extra_observation_duration| to the observation window of a given
  // feature if it hasn't been used yet, do nothing otherwise.
  static void IncrementFeatureObservationDuration(
      SiteCharacteristicsFeatureProto* feature_proto,
      base::TimeDelta extra_observation_duration);

  // Initialize a SiteCharacteristicsFeatureProto object with its default
  // values.
  static void InitSiteCharacteristicsFeatureProtoWithDefaultValues(
      SiteCharacteristicsFeatureProto* proto);

  // Initialize this object with default values. If
  // |only_init_uninitialized_fields| is set to true then only the fields that
  // haven't yet been initialized will be initialized, otherwise everything will
  // be overriden with default values.
  // NOTE: Do not call this directly while the site is loaded as this will not
  // properly update the last_loaded time, instead call |ClearObservations|.
  void InitWithDefaultValues(bool only_init_uninitialized_fields);

  // Clear all the past observations about this site and invalidate the pending
  // read observations from the database.
  void ClearObservationsAndInvalidateReadOperation();

  // Returns the usage of |site_feature| for this site.
  SiteFeatureUsage GetFeatureUsage(
      const SiteCharacteristicsFeatureProto& feature_proto,
      const base::TimeDelta min_obs_time) const;

  // Helper function to update a given |SiteCharacteristicsFeatureProto| when a
  // feature gets used.
  void NotifyFeatureUsage(SiteCharacteristicsFeatureProto* feature_proto,
                          const char* feature_name);

  bool IsLoaded() const { return loaded_tabs_count_ > 0U; }

  // Callback that needs to be called by the database once it has finished
  // trying to read the protobuf.
  void OnInitCallback(
      base::Optional<SiteCharacteristicsProto> site_characteristic_proto);

  // Decrement the |loaded_tabs_in_background_count_| counter and update the
  // local feature observation durations if necessary.
  void DecrementNumLoadedBackgroundTabs();

  // Flush any state that's maintained in member variables to the proto.
  const SiteCharacteristicsProto& FlushStateToProto();

  // This site's characteristics, contains the features and other values are
  // measured.
  SiteCharacteristicsProto site_characteristics_;

  // The in-memory storage for the moving performance averages.
  ExponentialMovingAverage cpu_usage_estimate_;
  ExponentialMovingAverage private_footprint_kb_estimate_;

  // This site's origin.
  const url::Origin origin_;

  // The number of loaded tabs for this origin. Several tabs with the
  // same origin might share the same instance of this object, this counter
  // will allow to properly update the observation time (starts when the first
  // tab gets loaded, stops when the last one gets unloaded).
  size_t loaded_tabs_count_;

  // Number of loaded tabs currently in background for this origin, the
  // implementation doesn't need to track unloaded tabs running in background.
  size_t loaded_tabs_in_background_count_;

  // The time at which the |loaded_tabs_in_background_count_| counter changed
  // from 0 to 1.
  base::TimeTicks background_session_begin_;

  // The database used to store the site characteristics, it should outlive
  // this object.
  LocalSiteCharacteristicsDatabase* const database_;

  // The delegate that should get notified when this object is about to get
  // destroyed, it should outlive this object.
  OnDestroyDelegate* const delegate_;

  // Indicates if this object has been fully initialized, either because the
  // read operation from the database has completed or because it has been
  // cleared.
  bool fully_initialized_;

  // Dirty bit, indicates if any of the fields in |site_characteristics_| has
  // changed since it has been initialized.
  bool is_dirty_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LocalSiteCharacteristicsDataImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataImpl);
};

}  // namespace internal
}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_IMPL_H_
