// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_TRACKERS_H_
#define CAST_COMMON_MDNS_MDNS_TRACKERS_H_

#include <random>
#include <unordered_map>

#include "cast/common/mdns/mdns_random.h"
#include "cast/common/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "util/alarm.h"

namespace cast {
namespace mdns {

using openscreen::Alarm;
using openscreen::platform::Clock;
using openscreen::platform::ClockNowFunctionPtr;
using openscreen::platform::TaskRunner;

// MdnsQuestionTracker manages automatic resending of mDNS queries for
// continuous monitoring with exponential back-off as described in RFC 6762
class MdnsQuestionTracker {
 public:
  // MdnsQuestionTracker does not own |sender|, |task_runner| and |random_delay|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsQuestionTracker.
  MdnsQuestionTracker(MdnsSender* sender,
                      TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function,
                      MdnsRandom* random_delay);

  MdnsQuestionTracker(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker(MdnsQuestionTracker&& other) noexcept = delete;
  ~MdnsQuestionTracker() = default;

  MdnsQuestionTracker& operator=(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker& operator=(MdnsQuestionTracker&& other) noexcept = delete;

  // Starts sending query messages for the provided question. Returns error with
  // code Error::Code::kOperationInvalid if called on an instance of
  // MdnsQuestionTracker that has already been started.
  Error Start(MdnsQuestion question);

  // Stop sending query messages and resets the querying interval. Returns error
  // with code Error::Code::kOperationInvalid if called on an instance of
  // MdnsQuestionTracker that has not yet been started or has already been
  // stopped.
  Error Stop();

  // Return true if MdnsQuestionTracker instance has been started and is
  // automatically sending queries, false otherwise.
  bool IsStarted();

 private:
  // Sends a query message via MdnsSender and schedules the next resend.
  void SendQuestion();

  MdnsSender* const sender_;
  const ClockNowFunctionPtr now_function_;
  Alarm resend_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* const random_delay_;

  // Stores MdnsQuestion provided to Start method call.
  absl::optional<MdnsQuestion> question_;
  Clock::duration resend_delay_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_TRACKERS_H_
