// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_publisher.h"

#include <chrono>
#include <cmath>

#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

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

inline MdnsRecord CreateGoodbyeRecord(const MdnsRecord& record) {
  return MdnsRecord(record.name(), record.dns_type(), record.dns_class(),
                    record.record_type(), kGoodbyeTtl, record.rdata());
}

inline void ValidateRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() != DnsType::kANY);
  OSP_DCHECK(record.dns_class() != DnsClass::kANY);
}

}  // namespace

MdnsPublisher::MdnsPublisher(MdnsQuerier* querier,
                             MdnsSender* sender,
                             TaskRunner* task_runner,
                             MdnsRandom* random_delay,
                             ClockNowFunctionPtr now_function)
    : querier_(querier),
      sender_(sender),
      task_runner_(task_runner),
      random_delay_(random_delay),
      now_function_(now_function) {
  // NOTE: |querier_| and |random_delay_| are intentionally not validated.
  // These are provided for future use in the Probe workflow.
  // As that feature is added and its requirements become clearer, these
  // validations will be added.
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);
}

MdnsPublisher::~MdnsPublisher() = default;

Error MdnsPublisher::RegisterRecord(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  ValidateRecord(record);

  return record.dns_type() == DnsType::kPTR ? RegisterPtrRecord(record)
                                            : RegisterNonPtrRecord(record);
}

Error MdnsPublisher::UnregisterRecord(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  ValidateRecord(record);

  return record.dns_type() == DnsType::kPTR ? UnregisterPtrRecord(record)
                                            : UnregisterNonPtrRecord(record);
}

Error MdnsPublisher::UpdateRegisteredRecord(const MdnsRecord& old_record,
                                            const MdnsRecord& new_record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

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

bool MdnsPublisher::IsExclusiveOwner(const DomainName& name) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  return records_.find(name) != records_.end();
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
  if (!IsExclusiveOwner(ptr_data.ptr_domain())) {
    return Error::Code::kParameterInvalid;
  }

  if (ptr_records_.find(record.name()) != ptr_records_.end()) {
    return Error::Code::kItemAlreadyExists;
  }

  ptr_records_.emplace(record.name(), CreateAnnouncer(std::move(record)));

  return Error::None();
}

Error MdnsPublisher::RegisterNonPtrRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() != DnsType::kPTR);

  const DomainName& name = record.name();
  if (!IsExclusiveOwner(name)) {
    return Error::Code::kParameterInvalid;
  }

  for (const auto& publisher : records_[name]) {
    if (publisher->record() == record) {
      return Error::Code::kItemAlreadyExists;
    }
  }

  records_[name].push_back(CreateAnnouncer(std::move(record)));

  return Error::None();
}

Error MdnsPublisher::UnregisterPtrRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() == DnsType::kPTR);

  // Check that |ptr_records_| has this domain and remove it if so.
  if (!ptr_records_.erase(record.name())) {
    return Error::Code::kItemNotFound;
  }

  // If this was the last record referring to the pointed-to domain, release
  // ownership of that name. A goodbye message will be sent for the removed
  // record, per RFC 6762 section 10.1.
  const DomainName& ptr_domain =
      absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
  const auto map_it = records_.find(ptr_domain);
  if (map_it != records_.end() && map_it->second.empty()) {
    records_.erase(map_it);
  }

  return Error::None();
}

Error MdnsPublisher::UnregisterNonPtrRecord(const MdnsRecord& record) {
  OSP_DCHECK(record.dns_type() != DnsType::kPTR);

  // Remove the Non-PTR record. A goodbye message will be sent for the removed
  // record, per RFC 6762 section 10.1.
  const Error remove_result = RemoveNonPtrRecord(record, true);
  if (!remove_result.ok()) {
    return remove_result;
  }

  // If all records registered to this domain were removed and no PTR records
  // point to this domain, release ownership over the name.
  const auto it = records_.find(record.name());
  if (it->second.empty()) {
    for (auto ptr_it = ptr_records_.begin(); ptr_it != ptr_records_.end();
         ptr_it++) {
      const DomainName& ptr_domain =
          absl::get<PtrRecordRdata>(ptr_it->second->record().rdata())
              .ptr_domain();
      if (ptr_domain == record.name()) {
        return Error::None();
      }
    }
    records_.erase(it);
  }

  return Error::None();
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

  return Error::None();
}

MdnsPublisher::RecordAnnouncer::RecordAnnouncer(
    MdnsRecord record,
    MdnsSender* sender,
    TaskRunner* task_runner,
    ClockNowFunctionPtr now_function)
    : sender_(sender),
      task_runner_(task_runner),
      now_function_(now_function),
      record_(std::move(record)),
      alarm_(now_function_, task_runner_) {
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);

  SendAnnouncement();
}

MdnsPublisher::RecordAnnouncer::~RecordAnnouncer() {
  alarm_.Cancel();
  if (should_send_goodbye_message_) {
    SendGoodbye();
  }
}

void MdnsPublisher::RecordAnnouncer::SendGoodbye() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  MdnsMessage message(CreateMessageId(), MessageType::Response);
  message.AddAnswer(CreateGoodbyeRecord(record_));
  sender_->SendMulticast(message);
}

void MdnsPublisher::RecordAnnouncer::SendAnnouncement() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  MdnsMessage message(CreateMessageId(), MessageType::Response);
  message.AddAnswer(record_);
  sender_->SendMulticast(message);

  const Clock::duration new_delay = GetNextAnnounceDelay();
  if (++attempts_ < kMaxAnnounceAttempts) {
    alarm_.ScheduleFromNow([this]() { SendAnnouncement(); }, new_delay);
  }
}

Clock::duration MdnsPublisher::RecordAnnouncer::GetNextAnnounceDelay() {
  return std::chrono::duration_cast<Clock::duration>(
      kMinAnnounceDelay * pow(kIntervalIncreaseFactor, attempts_));
}

}  // namespace discovery
}  // namespace openscreen
