// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_COMMON_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_COMMON_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

namespace profiles {
// Constants for the categories in ProfileCategoryStats
extern const char kProfileStatisticsBrowsingHistory[];
extern const char kProfileStatisticsPasswords[];
extern const char kProfileStatisticsBookmarks[];
extern const char kProfileStatisticsSettings[];

// Definition of a single return value of |ProfileStatisticsCallback|. If
// |success| is false, the statistics failed to load and |count| is undefined.
// The data look like these: {"BrowsingHistory", 912, true},
// {"Passwords", 71, true}, {"Bookmarks", 120, true}, {"Settings", 200, true}
struct ProfileCategoryStat {
  std::string category;
  int count;
  bool success;
};

// Definition of the return value of |ProfileStatisticsCallback|.
using ProfileCategoryStats = std::vector<ProfileCategoryStat>;

// Definition of the callback function. Note that a copy of
// |ProfileCategoryStats| is made each time the callback is called.
using ProfileStatisticsCallback = base::Callback<void(ProfileCategoryStats)>;
}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_COMMON_H_
