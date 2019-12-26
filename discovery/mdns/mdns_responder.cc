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

enum AddResult { kNonePresent = 0, kAdded, kAlreadyKnown };

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
  const auto records = record_handler->GetRecords(domain, type, clazz);
  if (records.size()) {
    bool added_any_records = false;
    for (const MdnsRecord& record : records) {
      if (std::find(known_answers.begin(), known_answers.end(), record) ==
          known_answers.end()) {
        added_any_records = true;
        add_func(record);
      }
    }
    return added_any_records ? AddResult::kAdded : AddResult::kAlreadyKnown;
  } else {
    if (add_negative_on_unknown) {
      // TODO(rwkeane): Add negative response NSEC record.
    }
    return AddResult::kNonePresent;
  }
}

inline AddResult AddAdditionalRecords(
    MdnsMessage* message,
    MdnsResponder::RecordHandler* record_handler,
    const DomainName& domain,
    const std::vector<MdnsRecord>& known_answers,
    DnsType type,
    DnsClass clazz) {
  OSP_DCHECK(IsValidAdditionalRecordType(type));

  auto add_func = [message](MdnsRecord record) {
    message->AddAdditionalRecord(std::move(record));
  };
  return AddRecords(std::move(add_func), record_handler, domain, known_answers,
                    type, clazz, true);
}

inline AddResult AddResponseRecords(
    MdnsMessage* message,
    MdnsResponder::RecordHandler* record_handler,
    const DomainName& domain,
    const std::vector<MdnsRecord>& known_answers,
    DnsType type,
    DnsClass clazz,
    bool add_negative_on_unknown) {
  OSP_DCHECK(type != DnsType::kANY);

  auto add_func = [message](MdnsRecord record) {
    message->AddAnswer(std::move(record));
  };
  return AddRecords(std::move(add_func), record_handler, domain, known_answers,
                    type, clazz, add_negative_on_unknown);
}

void ApplyAnyQueryResults(MdnsMessage* message,
                          MdnsResponder::RecordHandler* record_handler,
                          const DomainName& domain,
                          const std::vector<MdnsRecord>& known_answers,
                          DnsClass clazz) {
  if (AddResponseRecords(message, record_handler, domain, known_answers,
                         DnsType::kPTR, clazz, false)) {
    return;
  }

  if (!AddResponseRecords(message, record_handler, domain, known_answers,
                          DnsType::kSRV, clazz, false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
  if (!AddResponseRecords(message, record_handler, domain, known_answers,
                          DnsType::kTXT, clazz, false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
  if (!AddResponseRecords(message, record_handler, domain, known_answers,
                          DnsType::kA, clazz, false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
  if (!AddResponseRecords(message, record_handler, domain, known_answers,
                          DnsType::kAAAA, clazz, false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
}

void ApplyTypedQueryResults(MdnsMessage* message,
                            MdnsResponder::RecordHandler* record_handler,
                            const DomainName& domain,
                            const std::vector<MdnsRecord>& known_answers,
                            DnsType type,
                            DnsClass clazz) {
  OSP_DCHECK(type != DnsType::kANY);

  if (type == DnsType::kPTR) {
    auto response_records = record_handler->GetRecords(domain, type, clazz);
    for (const MdnsRecord& record : response_records) {
      if (std::find(known_answers.begin(), known_answers.end(), record) !=
          known_answers.end()) {
        continue;
      }

      message->AddAnswer(record);

      const DomainName& name =
          absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
      AddAdditionalRecords(message, record_handler, name, known_answers,
                           DnsType::kSRV, clazz);
      auto start_it = message->additional_records().begin();
      auto end_it = message->additional_records().end();
      for (auto it = start_it; it != end_it; it++) {
        const DomainName& target =
            absl::get<SrvRecordRdata>(it->rdata()).target();
        AddAdditionalRecords(message, record_handler, target, known_answers,
                             DnsType::kA, clazz);
        AddAdditionalRecords(message, record_handler, target, known_answers,
                             DnsType::kAAAA, clazz);
      }
      AddAdditionalRecords(message, record_handler, name, known_answers,
                           DnsType::kTXT, clazz);
    }
  } else {
    if (AddResponseRecords(message, record_handler, domain, known_answers, type,
                           clazz, true) == AddResult::kAdded) {
      if (type == DnsType::kSRV) {
        for (const auto& srv_record : message->answers()) {
          const DomainName& target =
              absl::get<SrvRecordRdata>(srv_record.rdata()).target();
          AddAdditionalRecords(message, record_handler, target, known_answers,
                               DnsType::kA, clazz);
          AddAdditionalRecords(message, record_handler, target, known_answers,
                               DnsType::kAAAA, clazz);
        }
      } else if (type == DnsType::kA) {
        AddAdditionalRecords(message, record_handler, domain, known_answers,
                             DnsType::kAAAA, clazz);
      } else if (type == DnsType::kAAAA) {
        AddAdditionalRecords(message, record_handler, domain, known_answers,
                             DnsType::kA, clazz);
      }
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

  const std::vector<MdnsRecord>& known_answers = message.answers();

  for (const auto& question : message.questions()) {
    const bool is_exclusive_owner =
        ownership_handler_->IsDomainClaimed(question.name());
    if (is_exclusive_owner ||
        record_handler_->HasRecords(question.name(), question.dns_type(),
                                    question.dns_class())) {
      std::function<void(const MdnsMessage&)> send_response;
      if (question.response_type() == ResponseType::kMulticast) {
        send_response = [this](const MdnsMessage& message) {
          sender_->SendMulticast(message);
        };
      } else {
        OSP_DCHECK(question.response_type() == ResponseType::kUnicast);
        send_response = [this, src](const MdnsMessage& message) {
          sender_->SendUnicast(message, src);
        };
      }

      if (is_exclusive_owner) {
        SendResponse(question, known_answers, std::move(send_response));
      } else {
        const auto delay = random_delay_->GetSharedRecordResponseDelay();
        std::function<void()> response = [this, question, known_answers,
                                          send = std::move(send_response)]() {
          SendResponse(question, known_answers, std::move(send));
        };
        task_runner_->PostTaskWithDelay(std::move(response), delay);
      }
    }
  }
}

void MdnsResponder::SendResponse(
    const MdnsQuestion& question,
    const std::vector<MdnsRecord>& known_answers,
    std::function<void(const MdnsMessage&)> send_response) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  MdnsMessage message(CreateMessageId(), MessageType::Response);

  if (question.dns_type() == DnsType::kANY) {
    ApplyAnyQueryResults(&message, record_handler_, question.name(),
                         known_answers, question.dns_class());
  } else {
    ApplyTypedQueryResults(&message, record_handler_, question.name(),
                           known_answers, question.dns_type(),
                           question.dns_class());
  }

  // Send the response only if there are any records we care about.
  if (!message.answers().empty()) {
    send_response(message);
  }
}

}  // namespace discovery
}  // namespace openscreen
