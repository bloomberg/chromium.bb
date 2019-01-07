// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_
#define CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_

#include "base/files/file_path.h"
#include "base/macros.h"

namespace usage_stats {

// UsageStatsDatabase will be used to store website events, suspensions and
// token to fdqn mappings in LevelDB
class UsageStatsDatabase {
 public:
  // Initializes the database with |database_folder|
  explicit UsageStatsDatabase(const base::FilePath& database_folder);

  ~UsageStatsDatabase();

  DISALLOW_COPY_AND_ASSIGN(UsageStatsDatabase);
};

}  // namespace usage_stats

#endif  // CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_
