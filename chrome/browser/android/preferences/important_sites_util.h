// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_IMPORTANT_SITES_UTIL_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_IMPORTANT_SITES_UTIL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

class Profile;

class ImportantSitesUtil {
 public:
  // This returns the top |<=max_results| important registerable
  // domains. This uses site engagement and notifications to generate the list.
  // |max_results| is assumed to be small.
  // If |optional_example_origins| is specified we populate that list with
  // example origins for each domain. This can be used to retrieve favicons.
  // See net/base/registry_controlled_domains/registry_controlled_domain.h for
  // more details on registrable domains and the current list of effective
  // eTLDs.
  static std::vector<std::string> GetImportantRegisterableDomains(
      Profile* profile,
      size_t max_results,
      std::vector<GURL>* optional_example_origins);

  static void RecordMetricsForBlacklistedSites(
      Profile* profile,
      std::vector<std::string> blacklisted_sites);

  // This marks the given origin as important so we can test features that rely
  // on important sites.
  static void MarkOriginAsImportantForTesting(Profile* profile,
                                              const GURL& origin);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ImportantSitesUtil);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_IMPORTANT_SITES_UTIL_H_
