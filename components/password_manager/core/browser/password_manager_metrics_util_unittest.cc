// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include <stddef.h>

#include <map>

#include "base/macros.h"
#include "base/values.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class PasswordManagerMetricsUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    prefs_.registry()->RegisterListPref(
        prefs::kPasswordManagerGroupsForDomains);
  }

  bool IsMonitored(const char* url_host) {
    size_t group_id = metrics_util::MonitoredDomainGroupId(url_host, &prefs_);
    return group_id > 0;
  }

  TestingPrefServiceSimple prefs_;
};

TEST_F(PasswordManagerMetricsUtilTest, MonitoredDomainGroupAssigmentTest) {
  const char* const kMonitoredWebsites[] = {
      "https://www.google.com",   "https://www.yahoo.com",
      "https://www.baidu.com",    "https://www.wikipedia.org",
      "https://www.linkedin.com", "https://www.twitter.com",
      "https://www.facebook.com", "https://www.amazon.com",
      "https://www.ebay.com",     "https://www.tumblr.com",
  };
  const size_t kMonitoredWebsitesLength = arraysize(kMonitoredWebsites);

  // |groups| maps the group id to the number of times the group gets assigned.
  std::map<size_t, size_t> groups;

  // Provide all possible values of the group id parameter for each monitored
  // website.
  for (size_t i = 0; i < kMonitoredWebsitesLength; ++i) {
    for (size_t j = 0; j < metrics_util::kGroupsPerDomain; ++j) {
      {  // Set the group index for domain |i| to |j|.
        ListPrefUpdate group_indices(&prefs_,
                                     prefs::kPasswordManagerGroupsForDomains);
        group_indices->Set(i, new base::FundamentalValue(static_cast<int>(j)));
      }  // At the end of the scope the prefs get updated.

      ++groups[metrics_util::MonitoredDomainGroupId(kMonitoredWebsites[i],
                                                    &prefs_)];
    }
  }
  // None of the monitored websites should have been assigned group ID 0.
  EXPECT_EQ(0u, groups.count(0));

  // Check if all groups get assigned the same number of times.
  size_t number_of_assigments = groups.begin()->second;
  for (auto it = groups.begin(); it != groups.end(); ++it) {
    EXPECT_EQ(it->second, number_of_assigments) << " group id = " << it->first;
  }
}

TEST_F(PasswordManagerMetricsUtilTest, MonitoredDomainGroupTest) {
  EXPECT_TRUE(IsMonitored("https://www.linkedin.com"));
  EXPECT_TRUE(IsMonitored("https://www.amazon.com"));
  EXPECT_TRUE(IsMonitored("https://www.facebook.com"));
  EXPECT_TRUE(IsMonitored("http://wikipedia.org"));
  EXPECT_FALSE(IsMonitored("http://thisisnotwikipedia.org"));
}

}  // namespace password_manager
