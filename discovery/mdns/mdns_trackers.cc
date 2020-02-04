// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_trackers.h"

#include <array>

#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_sender.h"
#include "util/std_util.h"

namespace openscreen {
namespace discovery {

namespace {

// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2

// Attempt to refresh a record should be performed at 80%, 85%, 90% and 95% TTL.
constexpr double kTtlFractions[] = {0.80, 0.85, 0.90, 0.95, 1.00};

// Intervals between successive queries must increase by at least a factor of 2.
constexpr int kIntervalIncreaseFactor = 2;

// The interval between the first two queries must be at least one second.
constexpr std::chrono::seconds kMinimumQueryInterval{1};

// The querier may cap the question refresh interval to a maximum of 60 minutes.
constexpr std::chrono::minutes kMaximumQueryInterval{60};

// RFC 6762 Section 10.1
// https://tools.ietf.org/html/rfc6762#section-10.1

// A goodbye record is a record with TTL of 0.
bool IsGoodbyeRecord(const MdnsRecord& record) {
  return record.ttl() == std::chrono::seconds{0};
}

// RFC 6762 Section 10.1
// https://tools.ietf.org/html/rfc6762#section-10.1
// In case of a goodbye record, the querier should set TTL to 1 second
constexpr std::chrono::seconds kGoodbyeRecordTtl{1};

}  // namespace

MdnsTracker::MdnsTracker(MdnsSender* sender,
                         TaskRunner* task_runner,
                         ClockNowFunctionPtr now_function,
                         MdnsRandom* random_delay)
    : sender_(sender),
      task_runner_(task_runner),
      now_function_(now_function),
      send_alarm_(now_function, task_runner),
      random_delay_(random_delay) {
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(now_function_);
  OSP_DCHECK(random_delay_);
  OSP_DCHECK(sender_);
}

MdnsTracker::~MdnsTracker() {
  send_alarm_.Cancel();

  for (MdnsTracker* node : adjacent_nodes_) {
    node->RemovedReverseAdjacency(this);
  }
}

bool MdnsTracker::AddAdjacentNode(MdnsTracker* node) {
  OSP_DCHECK(node);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto it = std::find(adjacent_nodes_.begin(), adjacent_nodes_.end(), node);
  if (it != adjacent_nodes_.end()) {
    return false;
  }

  node->AddReverseAdjacency(this);
  adjacent_nodes_.push_back(node);
  return true;
}

bool MdnsTracker::RemoveAdjacentNode(MdnsTracker* node) {
  OSP_DCHECK(node);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto it = std::find(adjacent_nodes_.begin(), adjacent_nodes_.end(), node);
  if (it == adjacent_nodes_.end()) {
    return false;
  }

  node->RemovedReverseAdjacency(this);
  adjacent_nodes_.erase(it);
  return true;
}

void MdnsTracker::AddReverseAdjacency(MdnsTracker* node) {
  OSP_DCHECK(std::find(adjacent_nodes_.begin(), adjacent_nodes_.end(), node) ==
             adjacent_nodes_.end());

  adjacent_nodes_.push_back(node);
}

void MdnsTracker::RemovedReverseAdjacency(MdnsTracker* node) {
  auto it = std::find(adjacent_nodes_.begin(), adjacent_nodes_.end(), node);
  OSP_DCHECK(it != adjacent_nodes_.end());

  adjacent_nodes_.erase(it);
}

MdnsRecordTracker::MdnsRecordTracker(
    MdnsRecord record,
    MdnsSender* sender,
    TaskRunner* task_runner,
    ClockNowFunctionPtr now_function,
    MdnsRandom* random_delay,
    std::function<void(const MdnsRecord&)> record_expired_callback)
    : MdnsTracker(sender, task_runner, now_function, random_delay),
      record_(std::move(record)),
      start_time_(now_function_()),
      record_expired_callback_(record_expired_callback) {
  OSP_DCHECK(record_expired_callback);

  ScheduleFollowUpQuery();
}

MdnsRecordTracker::~MdnsRecordTracker() = default;

ErrorOr<MdnsRecordTracker::UpdateType> MdnsRecordTracker::Update(
    const MdnsRecord& new_record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  bool has_same_rdata = record_.rdata() == new_record.rdata();

  // Goodbye records must have the same RDATA but TTL of 0.
  // RFC 6762 Section 10.1
  // https://tools.ietf.org/html/rfc6762#section-10.1
  if ((record_.dns_type() != new_record.dns_type()) ||
      (record_.dns_class() != new_record.dns_class()) ||
      (record_.name() != new_record.name()) ||
      (IsGoodbyeRecord(new_record) && !has_same_rdata)) {
    // The new record has been passed to a wrong tracker.
    return Error::Code::kParameterInvalid;
  }

  UpdateType result = UpdateType::kGoodbye;
  if (IsGoodbyeRecord(new_record)) {
    record_ = MdnsRecord(new_record.name(), new_record.dns_type(),
                         new_record.dns_class(), new_record.record_type(),
                         kGoodbyeRecordTtl, new_record.rdata());

    // Goodbye records do not need to be re-queried, set the attempt count to
    // the last item, which is 100% of TTL, i.e. record expiration.
    attempt_count_ = countof(kTtlFractions) - 1;
  } else {
    record_ = new_record;
    attempt_count_ = 0;
    result = has_same_rdata ? UpdateType::kTTLOnly : UpdateType::kRdata;
  }

  start_time_ = now_function_();
  ScheduleFollowUpQuery();

  return result;
}

bool MdnsRecordTracker::AddAssociatedQuery(
    MdnsQuestionTracker* question_tracker) {
  return AddAdjacentNode(question_tracker);
}

bool MdnsRecordTracker::RemoveAssociatedQuery(
    MdnsQuestionTracker* question_tracker) {
  return RemoveAdjacentNode(question_tracker);
}

void MdnsRecordTracker::ExpireSoon() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  record_ =
      MdnsRecord(record_.name(), record_.dns_type(), record_.dns_class(),
                 record_.record_type(), kGoodbyeRecordTtl, record_.rdata());

  // Set the attempt count to the last item, which is 100% of TTL, i.e. record
  // expiration, to prevent any re-queries
  attempt_count_ = countof(kTtlFractions) - 1;
  start_time_ = now_function_();
  ScheduleFollowUpQuery();
}

bool MdnsRecordTracker::IsNearingExpiry() {
  return (now_function_() - start_time_) > record_.ttl() / 2;
}

bool MdnsRecordTracker::SendQuery() {
  const Clock::time_point expiration_time = start_time_ + record_.ttl();
  bool is_expired = (now_function_() >= expiration_time);
  if (!is_expired) {
    for (MdnsTracker* tracker : adjacent_nodes()) {
      tracker->SendQuery();
    }
  } else {
    record_expired_callback_(record_);
  }

  return !is_expired;
}

void MdnsRecordTracker::ScheduleFollowUpQuery() {
  send_alarm_.Schedule(
      [this] {
        if (SendQuery()) {
          ScheduleFollowUpQuery();
        }
      },
      GetNextSendTime());
}

Clock::time_point MdnsRecordTracker::GetNextSendTime() {
  OSP_DCHECK(attempt_count_ < countof(kTtlFractions));

  double ttl_fraction = kTtlFractions[attempt_count_++];

  // Do not add random variation to the expiration time (last fraction of TTL)
  if (attempt_count_ != countof(kTtlFractions)) {
    ttl_fraction += random_delay_->GetRecordTtlVariation();
  }

  const Clock::duration delay =
      std::chrono::duration_cast<Clock::duration>(record_.ttl() * ttl_fraction);
  return start_time_ + delay;
}

MdnsQuestionTracker::MdnsQuestionTracker(MdnsQuestion question,
                                         MdnsSender* sender,
                                         TaskRunner* task_runner,
                                         ClockNowFunctionPtr now_function,
                                         MdnsRandom* random_delay,
                                         QueryType query_type)
    : MdnsTracker(sender, task_runner, now_function, random_delay),
      question_(std::move(question)),
      send_delay_(kMinimumQueryInterval),
      query_type_(query_type) {
  // Initialize the last send time to time_point::min() so that the next call to
  // SendQuery() is guaranteed to query the network.
  last_send_time_ = TrivialClockTraits::time_point::min();

  // The initial query has to be sent after a random delay of 20-120
  // milliseconds.
  if (query_type_ == QueryType::kOneShot) {
    task_runner_->PostTask([this] { MdnsQuestionTracker::SendQuery(); });
  } else {
    OSP_DCHECK(query_type_ == QueryType::kContinuous);
    send_alarm_.ScheduleFromNow(
        [this]() {
          MdnsQuestionTracker::SendQuery();
          ScheduleFollowUpQuery();
        },
        random_delay_->GetInitialQueryDelay());
  }
}

MdnsQuestionTracker::~MdnsQuestionTracker() = default;

bool MdnsQuestionTracker::AddAssociatedRecord(
    MdnsRecordTracker* record_tracker) {
  return AddAdjacentNode(record_tracker);
}

bool MdnsQuestionTracker::RemoveAssociatedRecord(
    MdnsRecordTracker* record_tracker) {
  return RemoveAdjacentNode(record_tracker);
}

bool MdnsQuestionTracker::SendQuery() {
  // NOTE: The RFC does not specify the minimum interval between queries for
  // multiple records of the same query when initiated for different reasons
  // (such as for different record refreshes or for one record refresh and the
  // periodic re-querying for a continuous query). For this reason, a constant
  // outside of scope of the RFC has been chosen.
  TrivialClockTraits::time_point now = now_function_();
  if (now < last_send_time_ + kMinimumQueryInterval) {
    return true;
  }
  last_send_time_ = now;

  MdnsMessage message(CreateMessageId(), MessageType::Query);
  message.AddQuestion(question_);

  // Send the message and additional known answer packets as needed.
  for (auto it = adjacent_nodes().begin(); it != adjacent_nodes().end();) {
    // NOTE: This cast is safe because AddAssocaitedRecord can only called on
    // MdnsRecordTracker objects and MdnsRecordTracker::AddAssociatedQuery() is
    // only called on MdnsQuestionTracker objects. This creates a bipartite
    // graph, where MdnsRecordTracker objects are only adjacent to
    // MdnsQuestionTracker objects and the opposite, so all nodes adjacent to
    // this one must be MdnsRecordTracker instances.
    MdnsRecordTracker* record_tracker = static_cast<MdnsRecordTracker*>(*it);
    if (record_tracker->IsNearingExpiry()) {
      it++;
      continue;
    }

    if (message.CanAddRecord(record_tracker->record())) {
      message.AddAnswer(record_tracker->record());
      it++;
    } else if (message.questions().empty() && message.answers().empty()) {
      // This case should never happen, because it means a record is too large
      // to fit into its own message.
      OSP_LOG << "Encountered unreasonably large message in cache. Skipping "
              << "known answer in suppressions...";
      it++;
    } else {
      message.set_truncated();
      sender_->SendMulticast(message);
      message = MdnsMessage(CreateMessageId(), MessageType::Query);
    }
  }
  sender_->SendMulticast(message);
  return true;
}

void MdnsQuestionTracker::ScheduleFollowUpQuery() {
  send_alarm_.ScheduleFromNow(
      [this] {
        if (SendQuery()) {
          ScheduleFollowUpQuery();
        }
      },
      send_delay_);
  send_delay_ = send_delay_ * kIntervalIncreaseFactor;
  if (send_delay_ > kMaximumQueryInterval) {
    send_delay_ = kMaximumQueryInterval;
  }
}

}  // namespace discovery
}  // namespace openscreen
