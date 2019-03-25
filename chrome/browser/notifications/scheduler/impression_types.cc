// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/impression_types.h"

#include <sstream>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace notifications {

bool Impression::operator==(const Impression& other) const {
  return create_time == other.create_time && feedback == other.feedback &&
         impression == other.impression && integrated == other.integrated;
}

SuppressionInfo::SuppressionInfo(const base::Time& last_trigger,
                                 const base::TimeDelta& duration)
    : last_trigger_time(last_trigger), duration(duration), recover_goal(1) {}

SuppressionInfo::SuppressionInfo(const SuppressionInfo& other) = default;

bool SuppressionInfo::operator==(const SuppressionInfo& other) const {
  return last_trigger_time == other.last_trigger_time &&
         duration == other.duration && recover_goal == other.recover_goal;
}

TypeState::TypeState(SchedulerClientType type)
    : type(type), current_max_daily_show(0) {}

TypeState::~TypeState() = default;

bool TypeState::operator==(const TypeState& other) const {
  return type == other.type &&
         current_max_daily_show == other.current_max_daily_show &&
         impressions == other.impressions &&
         suppression_info == other.suppression_info;
}

std::string TypeState::DebugPrint() const {
  std::string log = base::StringPrintf(
      "Type state: type: %d \n"
      "current_max_daily_show: %d \n"
      "impressions.size(): %zu \n",
      static_cast<int>(type), current_max_daily_show, impressions.size());

  for (const auto& it : impressions) {
    const auto& impression = it.second;
    std::ostringstream stream;
    stream << "Impression, create_time:" << impression.create_time << " \n"
           << "feedback: " << static_cast<int>(impression.feedback) << " \n"
           << "impression result: " << static_cast<int>(impression.impression)
           << " \n"
           << "integrated: " << impression.integrated << " \n";
    log += stream.str();
  }

  return log;
}

}  // namespace notifications
