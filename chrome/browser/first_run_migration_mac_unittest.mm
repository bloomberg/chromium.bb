// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// This is a test for temporary code, see chrome/browser/first_run_mac.mm
// for details.

namespace old_first_run_mac {

bool IsOldChromeFirstRunFromDictionary(NSDictionary *dict,
                                       bool *usage_stats_enabled);

extern const NSString *kOldUsageStatsPrefName;
}  // namespace google_update


class FirstRunMigrationTest : public PlatformTest {
};

TEST_F(FirstRunMigrationTest, MigrateOldFirstRunSettings) {
  using old_first_run_mac::IsOldChromeFirstRunFromDictionary;
  using old_first_run_mac::kOldUsageStatsPrefName;

  // Stats are off by default.
  bool stats_on;
  NSDictionary* empty_dict = [NSDictionary dictionary];
  EXPECT_FALSE(IsOldChromeFirstRunFromDictionary(empty_dict, &stats_on));
  EXPECT_FALSE(stats_on);

  // Stats reporting is ON.
  NSNumber* stats_enabled = [NSNumber numberWithBool:YES];
  NSDictionary* enabled_dict = [NSDictionary
      dictionaryWithObject:stats_enabled
                    forKey:kOldUsageStatsPrefName];
  EXPECT_TRUE(IsOldChromeFirstRunFromDictionary(enabled_dict, &stats_on));
  EXPECT_TRUE(stats_on);

  // Stats reporting is OFF.
  stats_enabled = [NSNumber numberWithBool:NO];
  enabled_dict = [NSDictionary
      dictionaryWithObject:stats_enabled
                    forKey:kOldUsageStatsPrefName];
  EXPECT_TRUE(IsOldChromeFirstRunFromDictionary(enabled_dict, &stats_on));
  EXPECT_FALSE(stats_on);


  // If an object of the wrong type is present, we still consider this  to
  // be a first run, but stats reporting is disabled.
  NSDictionary* wrong_type_dict = [NSDictionary
      dictionaryWithObject:empty_dict
                    forKey:kOldUsageStatsPrefName];
  EXPECT_TRUE(IsOldChromeFirstRunFromDictionary(wrong_type_dict, &stats_on));
  EXPECT_FALSE(stats_on);
}
