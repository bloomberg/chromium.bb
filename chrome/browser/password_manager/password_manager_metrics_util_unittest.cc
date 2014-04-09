// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include <iterator>
#include <map>

#include "base/basictypes.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordManagerMetricsUtilTest : public testing::Test {
 public:
  PasswordManagerMetricsUtilTest() {}

 protected:
  bool IsMonitored(const char* url_host) {
    size_t group_id = password_manager::metrics_util::MonitoredDomainGroupId(
        url_host, profile_.GetPrefs());
    return group_id > 0;
  }

  TestingProfile profile_;
};

TEST_F(PasswordManagerMetricsUtilTest, MonitoredDomainGroupAssigmentTest) {
  const char* const kMonitoredWebsites[] = {
      "https://www.google.com",
      "https://www.yahoo.com",
      "https://www.baidu.com",
      "https://www.wikipedia.org",
      "https://www.linkedin.com",
      "https://www.twitter.com",
      "https://www.facebook.com",
      "https://www.amazon.com",
      "https://www.ebay.com",
      "https://www.tumblr.com",
  };
  const size_t kMonitoredWebsitesLength = arraysize(kMonitoredWebsites);

  // The |groups| map contains the group id and the number of times
  // it get assigned.
  std::map<size_t, size_t> groups;

  // Provide all possible values of the group id parameter for each monitored
  // website.
  for (size_t i = 0; i < kMonitoredWebsitesLength; ++i) {
    for (size_t j = 0; j < password_manager::metrics_util::kGroupsPerDomain;
         ++j) {
      {  // Set the group index for domain |i| to |j|.
        ListPrefUpdate group_indices(
            profile_.GetPrefs(),
            password_manager::prefs::kPasswordManagerGroupsForDomains);
        group_indices->Set(i, new base::FundamentalValue(static_cast<int>(j)));
      }  // At the end of the scope the prefs get updated.

      ++groups[password_manager::metrics_util::MonitoredDomainGroupId(
          kMonitoredWebsites[i], profile_.GetPrefs())];
    }
  }

  // Check if all groups get assigned the same number of times.
  size_t number_of_assigment = groups.begin()->second;
  for (std::map<size_t, size_t>::iterator it = groups.begin();
       it != groups.end(); ++it) {
    EXPECT_EQ(it->second, number_of_assigment) << " group id = " << it->first;
  }
}

TEST_F(PasswordManagerMetricsUtilTest, MonitoredDomainGroupTest) {
  EXPECT_TRUE(IsMonitored("https://www.linkedin.com"));
  EXPECT_TRUE(IsMonitored("https://www.amazon.com"));
  EXPECT_TRUE(IsMonitored("https://www.facebook.com"));
  EXPECT_TRUE(IsMonitored("http://wikipedia.org"));
  EXPECT_FALSE(IsMonitored("http://thisisnotwikipedia.org"));
}
