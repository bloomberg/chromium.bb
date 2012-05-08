// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_SERVICE_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_SERVICE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/metrics/proto/study.pb.h"
#include "chrome/browser/metrics/proto/trials_seed.pb.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/common/url_fetcher_delegate.h"

template <typename T> struct DefaultSingletonTraits;
class PrefService;

// Used to setup field trials based on stored variations seed data, and fetch
// new seed data from the variations server.
class VariationsService : public content::URLFetcherDelegate {
 public:
  // Returns the singleton instance;
  static VariationsService* GetInstance();

  // Loads the Variations seed data from the given local prefs. If there is a
  // problem with loading, the pref value is cleared.
  void LoadVariationsSeed(PrefService* local_prefs);

  // Starts the fetching process, where |OnURLFetchComplete| is called with the
  // response.
  void StartFetchingVariationsSeed();

  // content::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // Register Variations related prefs in Local State.
  static void RegisterPrefs(PrefService* prefs);

 private:
  friend struct DefaultSingletonTraits<VariationsService>;

  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyChannel);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyVersion);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyVersionWildcards);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CheckStudyDate);

  VariationsService();
  virtual ~VariationsService();

  // Store the given seed data to the given local prefs. Note that |seed_data|
  // is assumed to be the raw serialized protobuf data stored in a string. It
  // will be Base64Encoded for storage. If the string is invalid or the encoding
  // fails, the |local_prefs| is left as is.
  void StoreSeedData(const std::string& seed_data, PrefService* local_prefs);

  // Returns whether |study| should be added to the local field trials list
  // according to its restriction parameters.
  static bool ShouldAddStudy(const chrome_variations::Study& study);

  // Checks whether |study| is applicable for the given |channel|.
  static bool CheckStudyChannel(const chrome_variations::Study& study,
                                chrome::VersionInfo::Channel channel);

  // Checks whether |study| is applicable for the given version string.
  static bool CheckStudyVersion(const chrome_variations::Study& study,
                                const std::string& version_string);

  // Checks whether |study| is applicable for the given date/time.
  static bool CheckStudyDate(const chrome_variations::Study& study,
                             const base::Time& date_time);

  // Contains the current seed request. Will only have a value while a request
  // is pending, and will be reset by |OnURLFetchComplete|.
  scoped_ptr<content::URLFetcher> pending_seed_request_;

  // The variations seed data being used for this session.
  // TODO(jwd): This should be removed. When the seed data is loaded, it will be
  // used immediately so it won't need to be stored.
  chrome_variations::TrialsSeed variations_seed_;
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_SERVICE_H_
