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

void AddImpressionTestData(const std::vector<ImpressionTestData>& test_data,
                           ImpressionHistoryTracker::TypeStates* type_states) {
  DCHECK(type_states);
  for (const auto& test_data : test_data) {
    auto type_state = std::make_unique<TypeState>(test_data.type);
    type_state->current_max_daily_show = test_data.current_max_daily_show;
    for (const auto& impression : test_data.impressions)
      type_state->impressions.emplace(impression.create_time, impression);
    type_state->suppression_info = test_data.suppression_info;

    type_states->emplace(test_data.type, std::move(type_state));
  }
}

}  // namespace test
}  // namespace notifications
