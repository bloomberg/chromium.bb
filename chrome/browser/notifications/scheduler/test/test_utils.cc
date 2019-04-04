// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/test/test_utils.h"

namespace notifications {
namespace test {

ImpressionTestData::ImpressionTestData(
    SchedulerClientType type,
    int current_max_daily_show,
    std::vector<Impression> impressions,
    base::Optional<SuppressionInfo> suppression_info)
    : type(type),
      current_max_daily_show(current_max_daily_show),
      impressions(std::move(impressions)),
      suppression_info(std::move(suppression_info)) {}

ImpressionTestData::ImpressionTestData(const ImpressionTestData& other) =
    default;

ImpressionTestData::~ImpressionTestData() = default;

void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data,
    ImpressionHistoryTracker::ClientStates* client_states) {
  DCHECK(client_states);
  for (const auto& test_data : test_data) {
    auto client_state = std::make_unique<ClientState>(test_data.type);
    client_state->current_max_daily_show = test_data.current_max_daily_show;
    for (const auto& impression : test_data.impressions) {
      client_state->impressions.emplace_back(impression);
    }
    client_state->suppression_info = test_data.suppression_info;
    client_states->emplace(test_data.type, std::move(client_state));
  }
}

}  // namespace test
}  // namespace notifications
