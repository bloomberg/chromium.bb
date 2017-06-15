// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/guid.h"
#include "components/download/internal/test/entry_utils.h"

namespace download {
namespace test {

bool CompareEntry(const Entry* const& expected, const Entry* const& actual) {
  if (expected == nullptr || actual == nullptr)
    return expected == actual;

  // TODO(shaktisahu): Add operator== in Entry.
  return expected->client == actual->client && expected->guid == actual->guid &&
         expected->scheduling_params.cancel_time ==
             actual->scheduling_params.cancel_time &&
         expected->scheduling_params.network_requirements ==
             actual->scheduling_params.network_requirements &&
         expected->scheduling_params.battery_requirements ==
             actual->scheduling_params.battery_requirements &&
         expected->scheduling_params.priority ==
             actual->scheduling_params.priority &&
         expected->request_params.url == actual->request_params.url &&
         expected->request_params.method == actual->request_params.method &&
         expected->request_params.request_headers.ToString() ==
             actual->request_params.request_headers.ToString() &&
         expected->state == actual->state;
}

bool CompareEntryList(const std::vector<Entry*>& expected,
                      const std::vector<Entry*>& actual) {
  return std::is_permutation(actual.cbegin(), actual.cend(), expected.cbegin(),
                             CompareEntry);
}

bool EntryComparison(const Entry& expected, const Entry& actual) {
  return CompareEntry(&expected, &actual);
}

bool CompareEntryList(const std::vector<Entry>& list1,
                      const std::vector<Entry>& list2) {
  return std::is_permutation(list1.begin(), list1.end(), list2.begin(),
                             EntryComparison);
}

Entry BuildBasicEntry() {
  return BuildEntry(DownloadClient::TEST, base::GenerateGUID());
}

Entry BuildBasicEntry(Entry::State state) {
  Entry entry = BuildBasicEntry();
  entry.state = state;
  return entry;
}

Entry BuildEntry(DownloadClient client, const std::string& guid) {
  Entry entry;
  entry.client = client;
  entry.guid = guid;
  return entry;
}

Entry BuildEntry(DownloadClient client,
                 const std::string& guid,
                 base::Time cancel_time,
                 SchedulingParams::NetworkRequirements network_requirements,
                 SchedulingParams::BatteryRequirements battery_requirements,
                 SchedulingParams::Priority priority,
                 const GURL& url,
                 const std::string& request_method,
                 Entry::State state) {
  Entry entry = BuildEntry(client, guid);
  entry.scheduling_params.cancel_time = cancel_time;
  entry.scheduling_params.network_requirements = network_requirements;
  entry.scheduling_params.battery_requirements = battery_requirements;
  entry.scheduling_params.priority = priority;
  entry.request_params.url = url;
  entry.request_params.method = request_method;
  entry.state = state;
  return entry;
}

}  // namespace test
}  // namespace download
