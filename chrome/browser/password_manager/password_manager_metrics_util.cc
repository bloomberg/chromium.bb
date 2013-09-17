// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_metrics_util.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace password_manager_metrics_util {

namespace {

// The number of domain groups.
const size_t kNumGroups = 2 * kGroupsPerDomain;

// The group index used to determine the group id attributed to the user
// in order to report the behaviour of the save password prompt. When
// |g_random_group_index| is equal to |kGroupsPerDomain| the index is
// generated randomly in range [0, |kGroupsPerDomain|). The user keeps his
// group index till the end of each session.
size_t g_random_group_index = kGroupsPerDomain;

// Contains a monitored domain name and all the group IDs which contain
// domain name.
struct DomainGroupsPair {
  const char* const domain_name;
  const size_t group_ids[kGroupsPerDomain];
};

// Generate a group index in range [0, |kGroupsPerDomain|).
// If the |g_random_group_index| is not equal to its initial value
// (|kGroupsPerDomain|), the value of |g_random_group_index| is not changed.
void GenerateRandomIndex() {
  // For each session, the user uses only one group for each website.
  if (g_random_group_index == kGroupsPerDomain)
    g_random_group_index = base::RandGenerator(kGroupsPerDomain);
}

}  // namespace

size_t MonitoredDomainGroupId(const std::string& url_host) {
  // This array contains each monitored website together with all ids of
  // groups which contain the website. Each website appears in
  // |kGroupsPerDomain| groups, and each group includes an equal number of
  // websites, so that no two websites have the same set of groups that they
  // belong to. All ids are in the range [1, |kNumGroups|].
  // For more information about the algorithm used see http://goo.gl/vUuFd5.
  static const DomainGroupsPair kDomainMapping[] = {
      {"google.com", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
      {"yahoo.com", {1, 2, 3, 4, 5, 11, 12, 13, 14, 15}},
      {"baidu.com", {1, 2, 3, 4, 6, 7, 11, 12, 16, 17}},
      {"wikipedia.org", {1, 2, 3, 4, 5, 6, 11, 12, 16, 18}},
      {"linkedin.com", {1, 6, 8, 11, 13, 14, 15, 16, 17, 19}},
      {"twitter.com", {5, 6, 7, 8, 9, 11, 13, 17, 19, 20}},
      {"live.com", {7, 8, 9, 10, 13, 14, 16, 17, 18, 20}},
      {"amazon.com", {2, 5, 9, 10, 12, 14, 15, 18, 19, 20}},
      {"ebay.com", {3, 7, 9, 10, 14, 15, 17, 18, 19, 20}},
      {"tumblr.com", {4, 8, 10, 12, 13, 15, 16, 18, 19, 20}},
  };
  const size_t kDomainMappingLength = arraysize(kDomainMapping);

  GenerateRandomIndex();

  GURL url(url_host);
  for (size_t i = 0; i < kDomainMappingLength; ++i) {
    if (url.DomainIs(kDomainMapping[i].domain_name))
      return kDomainMapping[i].group_ids[g_random_group_index];
  }
  return 0;
}

void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value) {
  DCHECK_LT(sample, boundary_value);

  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void LogUMAHistogramBoolean(const std::string& name, bool sample) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::BooleanHistogram::FactoryGet(
          name,
          base::Histogram::kNoFlags);
          histogram->AddBoolean(sample);
}

void SetRandomIndexForTesting(size_t value) {
  g_random_group_index = value;
}

std::string GroupIdToString(size_t group_id) {
  DCHECK_LE(group_id, kNumGroups);
  if (group_id > 0)
    return "group_" + base::IntToString(group_id);
  return std::string();
}

}  // namespace password_manager_metrics_util
