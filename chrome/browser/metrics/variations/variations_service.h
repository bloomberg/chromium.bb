// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SERVICE_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/variations/variations_request_scheduler.h"
#include "chrome/browser/web_resource/resource_request_allowed_notifier.h"
#include "chrome/common/chrome_version_info.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/metrics/variations/variations_registry_syncer_win.h"
#endif

class PrefService;
class PrefRegistrySimple;

namespace chrome_variations {

class TrialsSeed;

// Used to setup field trials based on stored variations seed data, and fetch
// new seed data from the variations server.
class VariationsService
    : public net::URLFetcherDelegate,
      public ResourceRequestAllowedNotifier::Observer {
 public:
  virtual ~VariationsService();

  // Creates field trials based on Variations Seed loaded from local prefs. If
  // there is a problem loading the seed data, all trials specified by the seed
  // may not be created.
  bool CreateTrialsFromSeed();

  // Calls FetchVariationsSeed once and repeats this periodically. See
  // implementation for details on the period. Must be called after
  // |CreateTrialsFromSeed|.
  void StartRepeatedVariationsSeedFetch();

  // Returns the variations server URL, which can vary if a command-line flag is
  // set and/or the variations restrict pref is set in |local_prefs|. Declared
  // static for test purposes.
  static GURL GetVariationsServerURL(PrefService* local_prefs);

#if defined(OS_WIN)
  // Starts syncing Google Update Variation IDs with the registry.
  void StartGoogleUpdateRegistrySync();
#endif

  // Exposed for testing.
  void SetCreateTrialsFromSeedCalledForTesting(bool called);

  // Exposed for testing.
  static std::string GetDefaultVariationsServerURLForTesting();

  // Register Variations related prefs in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Factory method for creating a VariationsService.
  static VariationsService* Create(PrefService* local_state);

 protected:
  // Starts the fetching process once, where |OnURLFetchComplete| is called with
  // the response.
  virtual void DoActualFetch();

  // This constructor exists for injecting a mock notifier. It is meant for
  // testing only. This instance will take ownership of |notifier|.
  VariationsService(ResourceRequestAllowedNotifier* notifier,
                    PrefService* local_state);

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, DoNotFetchIfOffline);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, DoNotFetchIfOnlineToOnline);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, FetchOnReconnect);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, LoadSeed);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, StoreSeed);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, SeedStoredWhenOKStatus);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, SeedNotStoredWhenNonOKStatus);

  // Creates the VariationsService with the given |local_state| prefs service.
  // Use the |Create| factory method to create a VariationsService.
  explicit VariationsService(PrefService* local_state);

  // Checks if prerequisites for fetching the Variations seed are met, and if
  // so, performs the actual fetch using |DoActualFetch|.
  void FetchVariationsSeed();

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // ResourceRequestAllowedNotifier::Observer implementation:
  virtual void OnResourceRequestsAllowed() OVERRIDE;

  // Store the given seed data to the given local prefs. Note that |seed_data|
  // is assumed to be the raw serialized protobuf data stored in a string. It
  // will be Base64Encoded for storage. If the string is invalid or the encoding
  // fails, the existing prefs are left as is and the function returns false.
  bool StoreSeedData(const std::string& seed_data, const base::Time& seed_date);

  // Loads the Variations seed data from local state into |seed|. If there is a
  // problem with loading, the pref value is cleared and false is returned. If
  // successful, |seed| will contain the loaded data and true is returned.
  bool LoadTrialsSeedFromPref(TrialsSeed* seed);

  // Record the time of the most recent successful fetch.
  void RecordLastFetchTime();

  // The pref service used to store persist the variations seed.
  PrefService* local_state_;

  // Contains the scheduler instance that handles timing for requests to the
  // server. Initially NULL and instantiated when the initial fetch is
  // requested.
  scoped_ptr<VariationsRequestScheduler> request_scheduler_;

  // Contains the current seed request. Will only have a value while a request
  // is pending, and will be reset by |OnURLFetchComplete|.
  scoped_ptr<net::URLFetcher> pending_seed_request_;

  // The URL to use for querying the Variations server.
  GURL variations_server_url_;

  // Cached serial number from the most recently fetched Variations seed.
  std::string variations_serial_number_;

  // Tracks whether |CreateTrialsFromSeed| has been called, to ensure that
  // it gets called prior to |StartRepeatedVariationsSeedFetch|.
  bool create_trials_from_seed_called_;

  // Tracks whether the initial request to the variations server had completed.
  bool initial_request_completed_;

  // Helper class used to tell this service if it's allowed to make network
  // resource requests.
  scoped_ptr<ResourceRequestAllowedNotifier> resource_request_allowed_notifier_;

  // The start time of the last seed request. This is used to measure the
  // latency of seed requests. Initially zero.
  base::TimeTicks last_request_started_time_;

#if defined(OS_WIN)
  // Helper that handles synchronizing Variations with the Registry.
  VariationsRegistrySyncer registry_syncer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VariationsService);
};

}  // namespace chrome_variations

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SERVICE_H_
