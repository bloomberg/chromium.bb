// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_QUERIER_H_
#define CAST_COMMON_MDNS_MDNS_QUERIER_H_

#include <unordered_map>

#include "absl/hash/hash.h"
#include "cast/common/mdns/mdns_records.h"
#include "platform/api/task_runner.h"

namespace cast {
namespace mdns {

class MdnsRandom;
class MdnsReceiver;
class MdnsRecordChangedCallback;
class MdnsSender;
class MdnsQuestionTracker;

class MdnsQuerier {
 public:
  using ClockNowFunctionPtr = openscreen::platform::ClockNowFunctionPtr;
  using TaskRunner = openscreen::platform::TaskRunner;

  MdnsQuerier(MdnsSender* sender,
              MdnsReceiver* receiver,
              TaskRunner* task_runner,
              ClockNowFunctionPtr now_function,
              MdnsRandom* random_delay);
  MdnsQuerier(const MdnsQuerier& other) = delete;
  MdnsQuerier(MdnsQuerier&& other) noexcept = delete;
  MdnsQuerier& operator=(const MdnsQuerier& other) = delete;
  MdnsQuerier& operator=(MdnsQuerier&& other) noexcept = delete;
  ~MdnsQuerier();

  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback);

  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback);

 private:
  using QuestionKey = std::tuple<DomainName, DnsType, DnsClass>;

  void OnMessageReceived(const MdnsMessage& message);

  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  TaskRunner* const task_runner_;
  const ClockNowFunctionPtr now_function_;
  MdnsRandom* const random_delay_;

  // Active question trackers, uniquely identified by domain name, DNS record
  // type and DNS record class. MdnsQuestionTracker instances are stored as
  // unique_ptr so they are not moved around in memory when the collection is
  // modified. This allows passing a pointer to MdnsQuestionTracker to a task
  // running on the TaskRunner.
  std::unordered_map<QuestionKey,
                     std::unique_ptr<MdnsQuestionTracker>,
                     absl::Hash<QuestionKey>>
      queries_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_QUERIER_H_
