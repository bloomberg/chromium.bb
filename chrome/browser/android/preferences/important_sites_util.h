// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_IMPORTANT_SITES_UTIL_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_IMPORTANT_SITES_UTIL_H_

#include <string>
#include <vector>

#include "base/macros.h"

class Profile;

class ImportantSitesUtil {
 public:
  // This returns the top |<=max_results| important registerable
  // domains. This uses site engagement and notifications to generate the list.
  // |max_results| is assumed to be small.
  // See net/base/registry_controlled_domains/registry_controlled_domain.h for
  // more details on registrable domains and the current list of effective
  // eTLDs.
  static std::vector<std::string> GetImportantRegisterableDomains(
      Profile* profile,
      size_t max_results);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ImportantSitesUtil);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_IMPORTANT_SITES_UTIL_H_
