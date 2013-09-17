// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <string>

namespace password_manager_metrics_util {

// We monitor the performance of the save password heuristic for a handful of
// domains. For privacy reasons we are not reporting UMA signals by domain, but
// by a domain group. A domain group can contain multiple domains, and a domain
// can be contained in multiple groups.
// For more information see http://goo.gl/vUuFd5.

// The number of groups in which each monitored website appears.
// It is a half of the total number of groups.
const size_t kGroupsPerDomain = 10u;

// Check whether the |url_host| is monitored or not. If yes, we return
// the id of the group which contains the domain name otherwise
// returns 0.
size_t MonitoredDomainGroupId(const std::string& url_host);

// A version of the UMA_HISTOGRAM_ENUMERATION macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value);

// A version of the UMA_HISTOGRAM_BOOLEAN macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramBoolean(const std::string& name, bool sample);

// Sets the random seed used to generate group ids for monitored domains.
void SetRandomIndexForTesting(size_t value);

// Returns a string which contains group_|group_id|. If the
// |group_id| corresponds to an unmonitored domain returns an empty string.
std::string GroupIdToString(size_t group_id);

}  // namespace password_manager_metrics_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_METRICS_UTIL_H_
