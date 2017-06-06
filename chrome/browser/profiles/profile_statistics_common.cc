// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_common.h"

namespace profiles {
const char kProfileStatisticsBrowsingHistory[] = "BrowsingHistory";
const char kProfileStatisticsPasswords[] = "Passwords";
const char kProfileStatisticsBookmarks[] = "Bookmarks";
const char kProfileStatisticsSettings[] = "Settings";

const std::array<const char*, 4> kProfileStatisticsCategories = {
    {kProfileStatisticsBrowsingHistory, kProfileStatisticsPasswords,
     kProfileStatisticsBookmarks, kProfileStatisticsSettings}};

}  // namespace profiles
