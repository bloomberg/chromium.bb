// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_publisher.h"

#include <chrono>
#include <cmath>

#include "discovery/mdns/mdns_probe_manager.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"
#include "platform/base/trivial_clock_traits.h"

namespace openscreen {
namespace discovery {
namespace {

// Minimum delay between announcements of a given record in seconds.
constexpr std::chrono::seconds kMinAnnounceDelay{1};

// Maximum number of resends for an announcement message.
constexpr int kMaxAnnounceAttempts = 8;

// Intervals between successive announcements must increase by at least a
// factor of 2.
constexpr int kIntervalIncreaseFactor = 2;

// TTL for a goodbye record in seconds. This constant is called out in RFC 6762
// section 10.1.
constexpr std::chrono::seconds kGoodbyeTtl{0};

// Timespan between sending batches of announcement and goodbye records, in
// microseconds.
constexpr Clock::duration kDelayBetweenBatchedRecords{20 * 1000};

inline MdnsRecord CreateGoodbyeRecord(const MdnsRecord& record) {
  if (record.ttl() == kGoodbyeTtl) {
    return record;
  }
  return MdnsRecord(record.name(), record.dns_type(), record.dns_class(),
                    record.record_type(), kGoodbyeTtl, record.rdata());
}

inline void ValidateRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() != DnsType::kANY);
  OSP_DCHECK(record.dns_class() != DnsClass::kANY);
}

// Tries to send the provided |message|, and splits it up and retries
// recursively if it fails. In practice, it should never fail (with <= 5
// records being sent, the message should be small), but since the message is
// created from a static vector, that's not guaranteed.
Error ProcessQueue(MdnsSender* sender, MdnsMessage* message) {
  Error send_response = sender->SendMulticast(*message);
  if (send_response.ok()) {
    return Error::None();
  } else if (message->answers().size() <= 1) {
    return send_response;
  }

  MdnsMessage first(message->id(), message->type());
  for (size_t i = 0; i < message->answers().size() / 2; i++) {
    first.AddAnswer(std::move(message->answers()[i]));
  }
  Error first_result = ProcessQueue(sender, &first);
  if (!first_result.ok()) {
    return first_result;
  }

  MdnsMessage second(CreateMessageId(), message->type());
  for (size_t i = message->answers().size() / 2; i < message->answers().size();
       i++) {
    second.AddAnswer(std::move(message->answers()[i]));
  }
  return ProcessQueue(sender, &second);
}

}  // namespace

MdnsPublisher::MdnsPublisher(MdnsSender* sender,
                             MdnsProbeManager* ownership_manager,
                             TaskRunner* task_runner,
                             ClockNowFunctionPtr now_function)
    : sender_(sender),
      ownership_manager_(ownership_manager),
      task_runner_(task_runner),
      now_function_(now_function) {
  OSP_DCHECK(ownership_manager_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);
}

MdnsPublisher::~MdnsPublisher() {
  if (batch_records_alarm_.has_value()) {
    batch_records_alarm_.value().Cancel();
    ProcessRecordQueue();
  }
}

Error MdnsPublisher::RegisterRecord(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  if (record.dns_type() == DnsType::kNSEC) {
    return Error::Code::kParameterInvalid;
  }
  ValidateRecord(record);

  return record.dns_type() == DnsType::kPTR ? RegisterPtrRecord(record)
                                            : RegisterNonPtrRecord(record);
}

Error MdnsPublisher::UnregisterRecord(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  if (record.dns_type() == DnsType::kNSEC) {
    return Error::Code::kParameterInvalid;
  }
  ValidateRecord(record);

  return record.dns_type() == DnsType::kPTR ? UnregisterPtrRecord(record)
                                            : UnregisterNonPtrRecord(record);
}

Error MdnsPublisher::UpdateRegisteredRecord(const MdnsRecord& old_record,
                                            const MdnsRecord& new_record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  if (old_record.dns_type() == DnsType::kNSEC) {
    return Error::Code::kParameterInvalid;
  }

  if (old_record.dns_type() == DnsType::kPTR) {
    return Error::Code::kParameterInvalid;
  }

  // Check that the old record and new record are compatible.
  if (old_record.name() != new_record.name() ||
      old_record.dns_type() != new_record.dns_type() ||
      old_record.dns_class() != new_record.dns_class() ||
      old_record.record_type() != new_record.record_type()) {
    return Error::Code::kParameterInvalid;
  }

  // Remove the old record. Per RFC 6762 section 8.4, a goodbye message will not
  // be sent, as all records which can be removed here are unique records, which
  // will be overwritten during the announcement phase when the updated record
  // is re-registered due to the cache-flush-bit's presence.
  const Error remove_result = RemoveNonPtrRecord(old_record, false);
  if (!remove_result.ok()) {
    return remove_result;
  }

  // Register the new record.
  return RegisterNonPtrRecord(new_record);
}

size_t MdnsPublisher::GetRecordCount() const {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  size_t count = ptr_records_.size();
  for (const auto& pair : records_) {
    count += pair.second.size();
  }

  return count;
}

bool MdnsPublisher::HasRecords(const DomainName& name,
                               DnsType type,
                               DnsClass clazz) {
  return !GetRecords(name, type, clazz).empty();
}

std::vector<MdnsRecord::ConstRef> MdnsPublisher::GetRecords(
    const DomainName& name,
    DnsType type,
    DnsClass clazz) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  std::vector<MdnsRecord::ConstRef> records;

  if (type != DnsType::kPTR) {
    auto it = records_.find(name);
    if (it != records_.end()) {
      for (const auto& publisher : it->second) {
        if ((type == DnsType::kANY || type == publisher->record().dns_type()) &&
            (clazz == DnsClass::kANY ||
             clazz == publisher->record().dns_class())) {
          records.push_back(publisher->record());
        }
      }
    }
  }

  if (type == DnsType::kPTR || type == DnsType::kANY) {
    const auto ptr_it = ptr_records_.find(name);
    if (ptr_it != ptr_records_.end()) {
      records.push_back(ptr_it->second->record());
    }
  }

  return records;
}

Error MdnsPublisher::RegisterPtrRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() == DnsType::kPTR);

  const auto& ptr_data = absl::get<PtrRecordRdata>(record.rdata());
  if (!ownership_manager_->IsDomainClaimed(ptr_data.ptr_domain())) {
    return Error::Code::kParameterInvalid;
  }

  auto pair = ptr_records_.emplace(record.name(), nullptr);
  if (!pair.second) {
    return Error::Code::kItemAlreadyExists;
  }

  auto announcer = CreateAnnouncer(std::move(record));
  pair.first->second.swap(announcer);
  return Error::None();
}

Error MdnsPublisher::RegisterNonPtrRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() != DnsType::kPTR);

  const DomainName& name = record.name();
  if (!ownership_manager_->IsDomainClaimed(name)) {
    return Error::Code::kParameterInvalid;
  }

  auto pair =
      records_.emplace(name, std::vector<std::unique_ptr<RecordAnnouncer>>{});
  for (const auto& publisher : pair.first->second) {
    if (publisher->record() == record) {
      return Error::Code::kItemAlreadyExists;
    }
  }

  pair.first->second.push_back(CreateAnnouncer(std::move(record)));

  return Error::None();
}

Error MdnsPublisher::UnregisterPtrRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() == DnsType::kPTR);

  // Check that |ptr_records_| has this domain and remove it if so.
  if (!ptr_records_.erase(record.name())) {
    return Error::Code::kItemNotFound;
  }

  return Error::None();
}

Error MdnsPublisher::UnregisterNonPtrRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() != DnsType::kPTR);

  // Remove the Non-PTR record. A goodbye message will be sent for the removed
  // record, per RFC 6762 section 10.1.
  return RemoveNonPtrRecord(record, true);
}

Error MdnsPublisher::RemoveNonPtrRecord(const MdnsRecord& record,
                                        bool should_announce_deletion) {
  OSP_DCHECK(record.dns_type() != DnsType::kPTR);

  // Check for the domain and fail if it's not found.
  const DomainName& name = record.name();
  const auto it = records_.find(name);
  if (it == records_.end()) {
    return Error::Code::kItemNotFound;
  }

  // Check for the record to be removed.
  const auto records_it = std::find_if(
      it->second.begin(), it->second.end(),
      [&record](const std::unique_ptr<RecordAnnouncer>& publisher) {
        return publisher->record() == record;
      });
  if (records_it == it->second.end()) {
    return Error::Code::kItemNotFound;
  }
  if (!should_announce_deletion) {
    (*records_it)->DisableGoodbyeMessageTransmission();
  }

  it->second.erase(records_it);
  if (it->second.empty()) {
    records_.erase(it);
  }

  return Error::None();
}

MdnsPublisher::RecordAnnouncer::RecordAnnouncer(
    MdnsRecord record,
    MdnsPublisher* publisher,
    TaskRunner* task_runner,
    ClockNowFunctionPtr now_function)
    : publisher_(publisher),
      task_runner_(task_runner),
      now_function_(now_function),
      record_(std::move(record)),
      alarm_(now_function_, task_runner_) {
  OSP_DCHECK(publisher_);
  OSP_DCHECK(task_runner_);

  QueueAnnouncement();
}

MdnsPublisher::RecordAnnouncer::~RecordAnnouncer() {
  alarm_.Cancel();
  if (should_send_goodbye_message_) {
    QueueGoodbye();
  }
}

void MdnsPublisher::RecordAnnouncer::QueueGoodbye() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  publisher_->QueueRecord(CreateGoodbyeRecord(record_));
}

void MdnsPublisher::RecordAnnouncer::QueueAnnouncement() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  publisher_->QueueRecord(record_);

  const Clock::duration new_delay = GetNextAnnounceDelay();
  if (++attempts_ < kMaxAnnounceAttempts) {
    alarm_.ScheduleFromNow([this]() { QueueAnnouncement(); }, new_delay);
  }
}

void MdnsPublisher::QueueRecord(MdnsRecord record) {
  if (!records_to_send_) {
    records_to_send_ = new std::vector<MdnsRecord>();
    batch_records_alarm_.emplace(now_function_, task_runner_);
    batch_records_alarm_.value().ScheduleFromNow(
        [this]() { ProcessRecordQueue(); }, kDelayBetweenBatchedRecords);
  }

  // Check that we aren't announcing and goodbye'ing a record in the same batch.
  // We expect to be sending no more than 5 records at a time, so don't worry
  // about iterating across this vector for each insert.
  auto goodbye = CreateGoodbyeRecord(record);
  auto existing_record_it =
      std::find_if(records_to_send_->begin(), records_to_send_->end(),
                   [&goodbye](const MdnsRecord& record) {
                     return goodbye == CreateGoodbyeRecord(record);
                   });

  // If we found it, only send the goodbye record. Else, simply add it to the
  // queue.
  if (existing_record_it == records_to_send_->end()) {
    records_to_send_->push_back(std::move(record));
  } else if (*existing_record_it == goodbye) {
    // This means that the goodbye record is already queued to be sent. This
    // means that there is no reason to also announce it, so exit early.
    return;
  } else if (record == goodbye) {
    // This means that we are sending a goodbye record right as it would also
    // be announced. Skip the announcement since the record is being
    // unregistered.
    *existing_record_it = std::move(record);
  } else if (record == *existing_record_it) {
    // This case shouldn't happen, but there is no work to do if it does. Log
    // to surface that something weird is going on.
    OSP_LOG_INFO << "Same record being announced multiple times.";
  } else {
    // This case should never occur. Support it just in case, but log to
    // surface that something weird is happening.
    OSP_LOG_INFO << "Updating the same record multiple times with multiple "
                    "TTL values.";
  }
}

// static
std::vector<MdnsRecord>* MdnsPublisher::records_to_send_ = nullptr;

void MdnsPublisher::ProcessRecordQueue() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  if (!records_to_send_) {
    return;
  }

  MdnsMessage message(CreateMessageId(), MessageType::Response);
  for (MdnsRecord& record : *records_to_send_) {
    message.AddAnswer(std::move(record));
  }
  ::openscreen::discovery::ProcessQueue(sender_, &message);

  batch_records_alarm_ = absl::nullopt;
  delete records_to_send_;
  records_to_send_ = nullptr;
}

Clock::duration MdnsPublisher::RecordAnnouncer::GetNextAnnounceDelay() {
  return std::chrono::duration_cast<Clock::duration>(
      kMinAnnounceDelay * pow(kIntervalIncreaseFactor, attempts_));
}

}  // namespace discovery
}  // namespace openscreen
