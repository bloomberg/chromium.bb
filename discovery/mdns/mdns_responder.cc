// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder.h"

#include <utility>

#include "discovery/mdns/mdns_probe_manager.h"
#include "discovery/mdns/mdns_publisher.h"
#include "discovery/mdns/mdns_querier.h"
#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {
namespace {

const std::array<std::string, 3> kServiceEnumerationDomainLabels{
    "_services", "_dns-sd", "_udp"};

enum AddResult { kNonePresent = 0, kAdded, kAlreadyKnown };

std::chrono::seconds GetTtlForRecordType(DnsType type) {
  switch (type) {
    case DnsType::kA:
      return kARecordTtl;
    case DnsType::kAAAA:
      return kAAAARecordTtl;
    case DnsType::kPTR:
      return kPtrRecordTtl;
    case DnsType::kSRV:
      return kSrvRecordTtl;
    case DnsType::kTXT:
      return kTXTRecordTtl;
    case DnsType::kANY:
      // If no records are present, re-querying should happen at the minimum
      // of any record that might be retrieved at that time.
      return kSrvRecordTtl;
    default:
      OSP_NOTREACHED();
      return std::chrono::seconds{0};
  }
}

MdnsRecord CreateNsecRecord(DomainName target_name,
                            DnsType target_type,
                            DnsClass target_class) {
  auto rdata = NsecRecordRdata(target_name, target_type);
  std::chrono::seconds ttl = GetTtlForRecordType(target_type);
  return MdnsRecord(std::move(target_name), DnsType::kNSEC, target_class,
                    RecordType::kUnique, ttl, std::move(rdata));
}

inline bool IsValidAdditionalRecordType(DnsType type) {
  return type == DnsType::kSRV || type == DnsType::kTXT ||
         type == DnsType::kA || type == DnsType::kAAAA;
}

AddResult AddRecords(std::function<void(MdnsRecord record)> add_func,
                     MdnsResponder::RecordHandler* record_handler,
                     const DomainName& domain,
                     const std::vector<MdnsRecord>& known_answers,
                     DnsType type,
                     DnsClass clazz,
                     bool add_negative_on_unknown) {
  auto records = record_handler->GetRecords(domain, type, clazz);
  if (records.empty()) {
    if (add_negative_on_unknown) {
      // TODO(rwkeane): Aggregate all NSEC records together into a single NSEC
      // record to reduce traffic.
      add_func(CreateNsecRecord(domain, type, clazz));
    }
    return AddResult::kNonePresent;
  } else {
    bool added_any_records = false;
    for (auto it = records.begin(); it != records.end(); it++) {
      if (std::find(known_answers.begin(), known_answers.end(), *it) ==
          known_answers.end()) {
        added_any_records = true;
        add_func(std::move(*it));
      }
    }
    return added_any_records ? AddResult::kAdded : AddResult::kAlreadyKnown;
  }
}

inline AddResult AddAdditionalRecords(
    MdnsMessage* message,
    MdnsResponder::RecordHandler* record_handler,
    const DomainName& domain,
    const std::vector<MdnsRecord>& known_answers,
    DnsType type,
    DnsClass clazz,
    bool add_negative_on_unknown) {
  OSP_DCHECK(IsValidAdditionalRecordType(type));

  auto add_func = [message](MdnsRecord record) {
    message->AddAdditionalRecord(std::move(record));
  };
  return AddRecords(std::move(add_func), record_handler, domain, known_answers,
                    type, clazz, add_negative_on_unknown);
}

inline AddResult AddResponseRecords(
    MdnsMessage* message,
    MdnsResponder::RecordHandler* record_handler,
    const DomainName& domain,
    const std::vector<MdnsRecord>& known_answers,
    DnsType type,
    DnsClass clazz,
    bool add_negative_on_unknown) {
  auto add_func = [message](MdnsRecord record) {
    message->AddAnswer(std::move(record));
  };
  return AddRecords(std::move(add_func), record_handler, domain, known_answers,
                    type, clazz, add_negative_on_unknown);
}

void ApplyQueryResults(MdnsMessage* message,
                       MdnsResponder::RecordHandler* record_handler,
                       const DomainName& domain,
                       const std::vector<MdnsRecord>& known_answers,
                       DnsType type,
                       DnsClass clazz,
                       bool is_exclusive_owner) {
  OSP_DCHECK(type != DnsType::kNSEC);

  // All records matching the provided query which have been published by this
  // host should be added to the response message per RFC 6762 section 6. If
  // this host is the exclusive owner of the queried domain name, then a
  // negative response NSEC record should be added in the case where the queried
  // record does not exist, per RFC 6762 section 6.1.
  if (AddResponseRecords(message, record_handler, domain, known_answers, type,
                         clazz, is_exclusive_owner) != AddResult::kAdded) {
    return;
  }

  // Per RFC 6763 section 12.1, when querying for a PTR record, all SRV records
  // and TXT records named in the PTR record's rdata should be added to the
  // messages additional records, as well as the address records of types A and
  // AAAA associated with the added SRV records. Per RFC 6762 section 6.1,
  // records with names matching those of reverse address mappings for PTR
  // records may be added as negative response NSEC records if they do not
  // exist.
  if (type == DnsType::kPTR) {
    // Add all SRV and TXT records to the additional records section.
    for (const MdnsRecord& record : message->answers()) {
      OSP_DCHECK(record.dns_type() == DnsType::kPTR);

      const DomainName& target =
          absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
      AddAdditionalRecords(message, record_handler, target, known_answers,
                           DnsType::kSRV, clazz, true);
      AddAdditionalRecords(message, record_handler, target, known_answers,
                           DnsType::kTXT, clazz, true);
    }

    // Add A and AAAA records associated with an added SRV record to the
    // additional records section.
    const int max = message->additional_records().size();
    for (int i = 0; i < max; i++) {
      if (message->additional_records()[i].dns_type() != DnsType::kSRV) {
        continue;
      }

      {
        const MdnsRecord& srv_record = message->additional_records()[i];
        const DomainName& target =
            absl::get<SrvRecordRdata>(srv_record.rdata()).target();
        AddAdditionalRecords(message, record_handler, target, known_answers,
                             DnsType::kA, clazz, target == domain);
      }

      // Must re-calculate the |srv_record|, |target| refs in case a resize of
      // the additional_records() vector has invalidated them.
      {
        const MdnsRecord& srv_record = message->additional_records()[i];
        const DomainName& target =
            absl::get<SrvRecordRdata>(srv_record.rdata()).target();
        AddAdditionalRecords(message, record_handler, target, known_answers,
                             DnsType::kAAAA, clazz, target == domain);
      }
    }
  }

  // Per RFC 6763 section 12.2, when querying for an SRV record, all address
  // records of type A and AAAA should be added to the additional records
  // section. Per RFC 6762 section 6.1, if these records are not present and
  // their name and class match that which is being queried for, a negative
  // response NSEC record may be added to show their non-existence.
  else if (type == DnsType::kSRV) {
    for (const auto& srv_record : message->answers()) {
      OSP_DCHECK(srv_record.dns_type() == DnsType::kSRV);

      const DomainName& target =
          absl::get<SrvRecordRdata>(srv_record.rdata()).target();
      AddAdditionalRecords(message, record_handler, target, known_answers,
                           DnsType::kA, clazz, target == domain);
      AddAdditionalRecords(message, record_handler, target, known_answers,
                           DnsType::kAAAA, clazz, target == domain);
    }
  }

  // Per RFC 6762 section 6.2, when querying for an address record of type A or
  // AAAA, the record of the opposite type should be added to the additional
  // records section if present. Else, a negative response NSEC record should be
  // added to show its non-existence.
  else if (type == DnsType::kA) {
    AddAdditionalRecords(message, record_handler, domain, known_answers,
                         DnsType::kAAAA, clazz, true);
  } else if (type == DnsType::kAAAA) {
    AddAdditionalRecords(message, record_handler, domain, known_answers,
                         DnsType::kA, clazz, true);
  }

  // The remaining supported records types are TXT, NSEC, and ANY. RFCs 6762 and
  // 6763 do not recommend sending any records in the additional records section
  // for queries of types TXT or ANY, and NSEC records are not supported for
  // queries.
}

// Determines if the provided query is a type enumeration query as described in
// RFC 6763 section 9.
bool IsServiceTypeEnumerationQuery(const MdnsQuestion& question) {
  if (question.dns_type() != DnsType::kPTR) {
    return false;
  }

  if (question.name().labels().size() <
      kServiceEnumerationDomainLabels.size()) {
    return false;
  }

  const auto question_it = question.name().labels().begin();
  return std::equal(question_it,
                    question_it + kServiceEnumerationDomainLabels.size(),
                    kServiceEnumerationDomainLabels.begin(),
                    kServiceEnumerationDomainLabels.end());
}

// Creates the expected response to a type enumeration query as described in RFC
// 6763 section 9.
void ApplyServiceTypeEnumerationResults(
    MdnsMessage* message,
    MdnsResponder::RecordHandler* record_handler,
    const DomainName& name,
    DnsClass clazz) {
  if (name.labels().size() < kServiceEnumerationDomainLabels.size()) {
    return;
  }

  std::vector<MdnsRecord::ConstRef> records =
      record_handler->GetPtrRecords(clazz);

  // skip "_services._dns-sd._udp." which was already checked for in above
  // method and just use the domain.
  const auto domain_it =
      name.labels().begin() + kServiceEnumerationDomainLabels.size();
  for (const MdnsRecord& record : records) {
    // Skip the 2 label service name in the PTR record's name.
    const auto record_it = record.name().labels().begin() + 2;
    if (std::equal(domain_it, name.labels().end(), record_it,
                   record.name().labels().end())) {
      message->AddAnswer(MdnsRecord(name, DnsType::kPTR, record.dns_class(),
                                    RecordType::kShared, record.ttl(),
                                    PtrRecordRdata(record.name())));
    }
  }
}

}  // namespace

MdnsResponder::MdnsResponder(RecordHandler* record_handler,
                             MdnsProbeManager* ownership_handler,
                             MdnsSender* sender,
                             MdnsReceiver* receiver,
                             TaskRunner* task_runner,
                             MdnsRandom* random_delay)
    : record_handler_(record_handler),
      ownership_handler_(ownership_handler),
      sender_(sender),
      receiver_(receiver),
      task_runner_(task_runner),
      random_delay_(random_delay) {
  OSP_DCHECK(record_handler_);
  OSP_DCHECK(ownership_handler_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(receiver_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);

  auto func = [this](const MdnsMessage& message, const IPEndpoint& src) {
    OnMessageReceived(message, src);
  };
  receiver_->SetQueryCallback(std::move(func));
}

MdnsResponder::~MdnsResponder() {
  receiver_->SetQueryCallback(nullptr);
}

MdnsResponder::RecordHandler::~RecordHandler() = default;

void MdnsResponder::OnMessageReceived(const MdnsMessage& message,
                                      const IPEndpoint& src) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Query);

  if (message.questions().empty()) {
    // TODO(rwkeane): Support multi-packet known answer suppression.
    return;
  }

  if (message.IsProbeQuery()) {
    ownership_handler_->RespondToProbeQuery(message, src);
    return;
  }

  OSP_DVLOG << "Received mDNS Query with " << message.questions().size()
            << " questions. Processing...";

  const std::vector<MdnsRecord>& known_answers = message.answers();

  for (const auto& question : message.questions()) {
    OSP_DVLOG << "\tProcessing mDNS Query for domain: '"
              << question.name().ToString() << "', type: '"
              << question.dns_type() << "'";

    // NSEC records should not be queried for.
    if (question.dns_type() == DnsType::kNSEC) {
      continue;
    }

    // Only respond to queries for which one of the following is true:
    // - This host is the sole owner of that domain.
    // - A record corresponding to this question has been published.
    // - The query is a service enumeration query.
    const bool is_service_enumeration = IsServiceTypeEnumerationQuery(question);
    const bool is_exclusive_owner =
        ownership_handler_->IsDomainClaimed(question.name());
    if (!is_service_enumeration && !is_exclusive_owner &&
        !record_handler_->HasRecords(question.name(), question.dns_type(),
                                     question.dns_class())) {
      OSP_DVLOG << "\tmDNS Query processed and no relevant records found!";
      continue;
    } else if (is_service_enumeration) {
      OSP_DVLOG << "\tmDNS Query is for service type enumeration!";
    }

    // Relevant records are published, so send them out using the response type
    // dictated in the question.
    std::function<void(const MdnsMessage&)> send_response;
    if (question.response_type() == ResponseType::kMulticast) {
      send_response = [this](const MdnsMessage& message) {
        sender_->SendMulticast(message);
      };
    } else {
      OSP_DCHECK(question.response_type() == ResponseType::kUnicast);
      send_response = [this, src](const MdnsMessage& message) {
        sender_->SendMessage(message, src);
      };
    }

    // If this host is the exclusive owner, respond immediately. Else, there may
    // be network contention if all hosts respond simultaneously, so delay the
    // response as dictated by RFC 6762.
    if (is_exclusive_owner) {
      SendResponse(question, known_answers, send_response, is_exclusive_owner);
    } else {
      const auto delay = random_delay_->GetSharedRecordResponseDelay();
      std::function<void()> response = [this, question, known_answers,
                                        send_response, is_exclusive_owner]() {
        SendResponse(question, known_answers, send_response,
                     is_exclusive_owner);
      };
      task_runner_->PostTaskWithDelay(response, delay);
    }
  }
}

void MdnsResponder::SendResponse(
    const MdnsQuestion& question,
    const std::vector<MdnsRecord>& known_answers,
    std::function<void(const MdnsMessage&)> send_response,
    bool is_exclusive_owner) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  MdnsMessage message(CreateMessageId(), MessageType::Response);

  if (IsServiceTypeEnumerationQuery(question)) {
    // This is a special case defined in RFC 6763 section 9, so handle it
    // separately.
    ApplyServiceTypeEnumerationResults(&message, record_handler_,
                                       question.name(), question.dns_class());
  } else {
    // NOTE: The exclusive ownership of this record cannot change before this
    // method is called. Exclusive ownership cannot be gained for a record which
    // has previously been published, and if this host is the exclusive owner
    // then this method will have been called without any delay on the task
    // runner
    ApplyQueryResults(&message, record_handler_, question.name(), known_answers,
                      question.dns_type(), question.dns_class(),
                      is_exclusive_owner);
  }

  // Send the response only if it contains answers to the query.
  if (!message.answers().empty()) {
    OSP_DVLOG << "\tmDNS Query processed and response sent!";
    send_response(message);
  } else {
    OSP_DVLOG << "\tmDNS Query processed and no response sent!";
  }
}

}  // namespace discovery
}  // namespace openscreen
