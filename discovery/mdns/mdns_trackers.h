// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_TRACKERS_H_
#define DISCOVERY_MDNS_MDNS_TRACKERS_H_

#include <unordered_map>

#include "absl/hash/hash.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"
#include "platform/base/trivial_clock_traits.h"
#include "util/alarm.h"

namespace openscreen {
namespace discovery {

class MdnsRandom;
class MdnsRecord;
class MdnsRecordChangedCallback;
class MdnsSender;

// MdnsTracker is a base class for MdnsRecordTracker and MdnsQuestionTracker for
// the purposes of common code sharing only.
//
// Instances of this class represent nodes of a bidirectional graph, such that
// if node A is adjacent to node B, B is also adjacent to A. In this class, the
// adjacent nodes are stored in adjacency list |associated_tracker_|, and
// exposed methods to add and remove nodes from this list also modify the added
// or removed node to remove this instance from its adjacency list.
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
  MdnsTracker& operator=(const MdnsTracker& other) = delete;
  MdnsTracker& operator=(MdnsTracker&& other) noexcept = delete;
  virtual ~MdnsTracker();

  // Sends a query message via MdnsSender. Returns false if a follow up query
  // should NOT be scheduled and true otherwise.
  virtual bool SendQuery() = 0;

 protected:
  // Schedules a repeat query to be sent out.
  virtual void ScheduleFollowUpQuery() = 0;

  // These methods create a bidirectional adjacency with another node in the
  // graph.
  bool AddAdjacentNode(MdnsTracker* tracker);
  bool RemoveAdjacentNode(MdnsTracker* tracker);

  const std::vector<MdnsTracker*>& adjacent_nodes() { return adjacent_nodes_; }

  MdnsSender* const sender_;
  TaskRunner* const task_runner_;
  const ClockNowFunctionPtr now_function_;
  Alarm send_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* const random_delay_;

 private:
  // These methods are used to ensure the bidirectional-ness of this graph.
  void AddReverseAdjacency(MdnsTracker* tracker);
  void RemovedReverseAdjacency(MdnsTracker* tracker);

  // Adjacency list for this graph node.
  std::vector<MdnsTracker*> adjacent_nodes_;
};

class MdnsQuestionTracker;

// MdnsRecordTracker manages automatic resending of mDNS queries for
// refreshing records as they reach their expiration time.
class MdnsRecordTracker : public MdnsTracker {
 public:
  MdnsRecordTracker(
      MdnsRecord record,
      MdnsSender* sender,
      TaskRunner* task_runner,
      ClockNowFunctionPtr now_function,
      MdnsRandom* random_delay,
      std::function<void(const MdnsRecord&)> record_expired_callback);

  ~MdnsRecordTracker() override;

  // Possible outcomes from updating a tracked record.
  enum class UpdateType {
    kGoodbye,  // The record has a TTL of 0 and will expire.
    kTTLOnly,  // The record updated its TTL only.
    kRdata     // The record updated its RDATA.
  };

  // Updates record tracker with the new record:
  // 1. Resets TTL to the value specified in |new_record|.
  // 2. Schedules expiration in case of a goodbye record.
  // Returns Error::Code::kParameterInvalid if new_record is not a valid update
  // for the current tracked record.
  ErrorOr<UpdateType> Update(const MdnsRecord& new_record);

  // Adds or removed a question which this record answers.
  bool AddAssociatedQuery(MdnsQuestionTracker* question_tracker);
  bool RemoveAssociatedQuery(MdnsQuestionTracker* question_tracker);

  // Sets record to expire after 1 seconds as per RFC 6762
  void ExpireSoon();

  // Returns true if half of the record's TTL has passed, and false otherwise.
  // Half is used due to specifications in RFC 6762 section 7.1.
  bool IsNearingExpiry();

  // Returns a reference to the tracked record.
  const MdnsRecord& record() const { return record_; }

 private:
  Clock::time_point GetNextSendTime();

  // MdnsTracker overrides.
  bool SendQuery() override;
  void ScheduleFollowUpQuery() override;

  // Stores MdnsRecord provided to Start method call.
  MdnsRecord record_;
  // A point in time when the record was received and the tracking has started.
  Clock::time_point start_time_;
  // Number of times record refresh has been attempted.
  size_t attempt_count_ = 0;
  std::function<void(const MdnsRecord&)> record_expired_callback_;
};

// MdnsQuestionTracker manages automatic resending of mDNS queries for
// continuous monitoring with exponential back-off as described in RFC 6762.
class MdnsQuestionTracker : public MdnsTracker {
 public:
  // Supported query types, per RFC 6762 section 5.
  enum class QueryType { kOneShot, kContinuous };

  MdnsQuestionTracker(MdnsQuestion question,
                      MdnsSender* sender,
                      TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function,
                      MdnsRandom* random_delay,
                      QueryType query_type = QueryType::kContinuous);

  ~MdnsQuestionTracker() override;

  // Adds or removed an answer to a the question posed by this tracker.
  bool AddAssociatedRecord(MdnsRecordTracker* record_tracker);
  bool RemoveAssociatedRecord(MdnsRecordTracker* record_tracker);

  // Returns a reference to the tracked question.
  const MdnsQuestion& question() const { return question_; }

 private:
  using RecordKey = std::tuple<DomainName, DnsType, DnsClass>;

  // Determines if all answers to this query have been received.
  bool HasReceivedAllResponses();

  // MdnsTracker overrides.
  bool SendQuery() override;
  void ScheduleFollowUpQuery() override;

  // Stores MdnsQuestion provided to Start method call.
  MdnsQuestion question_;

  // A delay between the currently scheduled and the next queries.
  Clock::duration send_delay_;

  // Last time that this tracker's question was asked.
  TrivialClockTraits::time_point last_send_time_;

  // Specifies whether this query is intended to be a one-shot query, as defined
  // in RFC 6762 section 5.1.
  const QueryType query_type_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_TRACKERS_H_
