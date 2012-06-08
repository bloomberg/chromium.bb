// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_SERVICE_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_SERVICE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/time.h"
#include "chrome/browser/metrics/proto/study.pb.h"
#include "chrome/browser/metrics/proto/trials_seed.pb.h"
#include "chrome/common/chrome_version_info.h"
#include "net/url_request/url_fetcher_delegate.h"

class PrefService;

namespace net {
class URLFetcher;
}  // namespace net

// Used to setup field trials based on stored variations seed data, and fetch
// new seed data from the variations server.
class VariationsService : public net::URLFetcherDelegate {
 public:
  VariationsService();
  virtual ~VariationsService();

  // Creates field trials based on Variations Seed loaded from local prefs. If
  // there is a problem loading the seed data, all trials specified by the seed
  // may not be created.
  bool CreateTrialsFromSeed(PrefService* local_prefs);

  // Starts the fetching process, where |OnURLFetchComplete| is called with the
  // response.
  void StartFetchingVariationsSeed();

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Register Variations related prefs in Local State.
  static void RegisterPrefs(PrefService* prefs);

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyChannel);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyPlatform);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyVersion);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyVersionWildcards);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyStartDate);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, IsStudyExpired);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, ValidateStudy);

  // Store the given seed data to the given local prefs. Note that |seed_data|
  // is assumed to be the raw serialized protobuf data stored in a string. It
  // will be Base64Encoded for storage. If the string is invalid or the encoding
  // fails, the |local_prefs| is left as is.
  void StoreSeedData(const std::string& seed_data,
                     const base::Time& seed_date,
                     PrefService* local_prefs);

  // Returns whether |study| should be disabled according to its restriction
  // parameters. Uses |version_info| for min / max version checks and
  // |reference_date| for the start date check.
  static bool ShouldAddStudy(const chrome_variations::Study& study,
                             const chrome::VersionInfo& version_info,
                             const base::Time& reference_date);

  // Checks whether a study is applicable for the given |channel| per |filter|.
  static bool CheckStudyChannel(const chrome_variations::Study_Filter& filter,
                                chrome::VersionInfo::Channel channel);

  // Checks whether a study is applicable for the given |platform| per |filter|.
  static bool CheckStudyPlatform(const chrome_variations::Study_Filter& filter,
                                 chrome_variations::Study_Platform platform);

  // Checks whether a study is applicable for the given version per |filter|.
  static bool CheckStudyVersion(const chrome_variations::Study_Filter& filter,
                                const std::string& version_string);

  // Checks whether a study is applicable for the given date/time per |filter|.
  static bool CheckStudyStartDate(const chrome_variations::Study_Filter& filter,
                                  const base::Time& date_time);

  // Checks whether |study| is expired using the given date/time.
  static bool IsStudyExpired(const chrome_variations::Study& study,
                             const base::Time& date_time);

  // Validates the sanity of |study| and computes the total probability.
  static bool ValidateStudyAndComputeTotalProbability(
      const chrome_variations::Study& study,
      base::FieldTrial::Probability* total_probability);

  // Loads the Variations seed data from the given local prefs into |seed|. If
  // there is a problem with loading, the pref value is cleared and false is
  // returned. If successful, |seed| will contain the loaded data and true is
  // returned.
  bool LoadTrialsSeedFromPref(PrefService* local_prefs,
                              chrome_variations::TrialsSeed* seed);

  // Creates and registers a field trial from the |study| data. Disables the
  // trial if IsStudyExpired(study, reference_date) is true.
  void CreateTrialFromStudy(const chrome_variations::Study& study,
                            const base::Time& reference_date);

  // Contains the current seed request. Will only have a value while a request
  // is pending, and will be reset by |OnURLFetchComplete|.
  scoped_ptr<net::URLFetcher> pending_seed_request_;
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_SERVICE_H_
