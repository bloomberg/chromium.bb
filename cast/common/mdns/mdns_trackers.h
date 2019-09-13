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

// MdnsTracker is a base class for MdnsRecordTracker and MdnsQuestionTracker for
// the purposes of common code sharing only
class MdnsTracker {
 public:
  // MdnsTracker does not own |sender|, |task_runner| and |random_delay|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsTracker.
  MdnsTracker(MdnsSender* sender,
              TaskRunner* task_runner,
              ClockNowFunctionPtr now_function,
              MdnsRandom* random_delay);

  MdnsTracker(const MdnsTracker& other) = delete;
  MdnsTracker(MdnsTracker&& other) noexcept = delete;
  ~MdnsTracker() = default;

  MdnsTracker& operator=(const MdnsTracker& other) = delete;
  MdnsTracker& operator=(MdnsTracker&& other) noexcept = delete;

 protected:
  MdnsSender* const sender_;
  const ClockNowFunctionPtr now_function_;
  Alarm send_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* const random_delay_;
};

// MdnsRecordTracker manages automatic resending of mDNS queries for
// refreshing records as they reach their expiration time.
class MdnsRecordTracker : public MdnsTracker {
 public:
  MdnsRecordTracker(
      MdnsSender* sender,
      TaskRunner* task_runner,
      ClockNowFunctionPtr now_function,
      MdnsRandom* random_delay,
      std::function<void(const MdnsRecord&)> record_updated_callback,
      std::function<void(const MdnsRecord&)> record_expired_callback);

  // Starts sending query messages for the provided record using record's TTL
  // and the time of the call to determine when to send the queries. Returns
  // error with code Error::Code::kOperationInvalid if called on an instance of
  // MdnsRecordTracker that has already been started.
  Error Start(MdnsRecord record);

  // Stops sending query for automatic record refresh. This cancels record
  // expiration notification as well. Returns error
  // with code Error::Code::kOperationInvalid if called on an instance of
  // MdnsRecordTracker that has not yet been started or has already been
  // stopped.
  Error Stop();

  // Updates record tracker with the new record:
  // 1. Calls update callback if RDATA has changed.
  // 2. Resets TTL to the value specified in new_record.
  // 3. Schedules expiration in case of a goodbye record.
  Error Update(const MdnsRecord& new_record);

  // Returns true if MdnsRecordTracker instance has been started and is
  // automatically refreshing the record, false otherwise.
  bool IsStarted();

 private:
  void SendQuery();
  Clock::time_point GetNextSendTime();

  // Stores MdnsRecord provided to Start method call.
  absl::optional<MdnsRecord> record_;
  // A point in time when the record was received and the tracking has started.
  Clock::time_point start_time_;
  // Number of times a question to refresh the record has been sent.
  size_t send_count_ = 0;
  std::function<void(const MdnsRecord&)> record_updated_callback_;
  std::function<void(const MdnsRecord&)> record_expired_callback_;
};

// MdnsQuestionTracker manages automatic resending of mDNS queries for
// continuous monitoring with exponential back-off as described in RFC 6762
class MdnsQuestionTracker : public MdnsTracker {
 public:
  MdnsQuestionTracker(MdnsSender* sender,
                      TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function,
                      MdnsRandom* random_delay);

  // Starts sending query messages for the provided question. Returns error with
  // code Error::Code::kOperationInvalid if called on an instance of
  // MdnsQuestionTracker that has already been started.
  Error Start(MdnsQuestion question);

  // Stops sending query messages and resets the querying interval. Returns
  // error with code Error::Code::kOperationInvalid if called on an instance of
  // MdnsQuestionTracker that has not yet been started or has already been
  // stopped.
  Error Stop();

  // Returns true if MdnsQuestionTracker instance has been started and is
  // automatically sending queries, false otherwise.
  bool IsStarted();

 private:
  // Sends a query message via MdnsSender and schedules the next resend.
  void SendQuery();

  // Stores MdnsQuestion provided to Start method call.
  absl::optional<MdnsQuestion> question_;
  Clock::duration send_delay_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_TRACKERS_H_
