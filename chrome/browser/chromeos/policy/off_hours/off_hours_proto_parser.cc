// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/off_hours/off_hours_proto_parser.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {
namespace off_hours {

std::vector<WeeklyTimeInterval> ExtractWeeklyTimeIntervalsFromProto(
    const em::DeviceOffHoursProto& container) {
  std::vector<WeeklyTimeInterval> intervals;
  for (const auto& entry : container.intervals()) {
    auto interval = WeeklyTimeInterval::ExtractFromProto(entry);
    if (interval)
      intervals.push_back(*interval);
  }
  return intervals;
}

std::vector<int> ExtractIgnoredPolicyProtoTagsFromProto(
    const em::DeviceOffHoursProto& container) {
  return std::vector<int>(container.ignored_policy_proto_tags().begin(),
                          container.ignored_policy_proto_tags().end());
}

base::Optional<std::string> ExtractTimezoneFromProto(
    const em::DeviceOffHoursProto& container) {
  if (!container.has_timezone()) {
    return base::nullopt;
  }
  return base::make_optional(container.timezone());
}

std::unique_ptr<base::DictionaryValue> ConvertOffHoursProtoToValue(
    const em::DeviceOffHoursProto& container) {
  base::Optional<std::string> timezone = ExtractTimezoneFromProto(container);
  if (!timezone)
    return nullptr;
  auto off_hours = std::make_unique<base::DictionaryValue>();
  off_hours->SetString("timezone", *timezone);
  std::vector<WeeklyTimeInterval> intervals =
      ExtractWeeklyTimeIntervalsFromProto(container);
  auto intervals_value = std::make_unique<base::ListValue>();
  for (const auto& interval : intervals)
    intervals_value->Append(interval.ToValue());
  off_hours->SetList("intervals", std::move(intervals_value));
  std::vector<int> ignored_policy_proto_tags =
      ExtractIgnoredPolicyProtoTagsFromProto(container);
  auto ignored_policies_value = std::make_unique<base::ListValue>();
  for (const auto& policy : ignored_policy_proto_tags)
    ignored_policies_value->GetList().emplace_back(policy);
  off_hours->SetList("ignored_policy_proto_tags",
                     std::move(ignored_policies_value));
  return off_hours;
}

}  // namespace off_hours
}  // namespace policy
