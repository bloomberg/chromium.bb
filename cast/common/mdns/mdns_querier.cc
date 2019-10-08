// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

#include "cast/common/mdns/mdns_random.h"
#include "cast/common/mdns/mdns_receiver.h"
#include "cast/common/mdns/mdns_sender.h"
#include "cast/common/mdns/mdns_trackers.h"

namespace cast {
namespace mdns {

MdnsQuerier::MdnsQuerier(MdnsSender* sender,
                         MdnsReceiver* receiver,
                         TaskRunner* task_runner,
                         ClockNowFunctionPtr now_function,
                         MdnsRandom* random_delay)
    : sender_(sender),
      receiver_(receiver),
      task_runner_(task_runner),
      now_function_(now_function),
      random_delay_(random_delay) {
  OSP_DCHECK(sender_);
  OSP_DCHECK(receiver_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(now_function_);
  OSP_DCHECK(random_delay_);

  receiver_->SetResponseCallback(
      [this](const MdnsMessage& message) { OnMessageReceived(message); });
}

MdnsQuerier::~MdnsQuerier() {
  receiver_->SetResponseCallback(nullptr);
}

void MdnsQuerier::StartQuery(const DomainName& name,
                             DnsType dns_type,
                             DnsClass dns_class,
                             MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(callback);

  const QuestionKey key(name, dns_type, dns_class);
  auto find_result = queries_.find(key);
  if (find_result == queries_.end()) {
    std::unique_ptr<MdnsQuestionTracker> tracker =
        std::make_unique<MdnsQuestionTracker>(sender_, task_runner_,
                                              now_function_, random_delay_);
    tracker->AddCallback(callback);
    tracker->Start(
        MdnsQuestion(name, dns_type, dns_class, ResponseType::kMulticast));
    queries_.emplace(key, std::move(tracker));
  } else {
    OSP_DCHECK(find_result->second->IsStarted());
    find_result->second->AddCallback(callback);
  }
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType dns_type,
                            DnsClass dns_class,
                            MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(callback);

  const QuestionKey key(name, dns_type, dns_class);
  auto find_result = queries_.find(key);
  if (find_result != queries_.end()) {
    MdnsQuestionTracker* query = find_result->second.get();
    query->RemoveCallback(callback);
    if (!query->HasCallbacks()) {
      queries_.erase(find_result);
    }
  }
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Response);

  // TODO(yakimakha): Check authority and additional records
  for (const MdnsRecord& record : message.answers()) {
    // TODO(yakimakha): Handle questions with type ANY and class ANY
    const QuestionKey key(record.name(), record.dns_type(), record.dns_class());
    auto find_result = queries_.find(key);
    if (find_result != queries_.end()) {
      MdnsQuestionTracker* query = find_result->second.get();
      query->OnRecordReceived(record);
    }
  }
}

}  // namespace mdns
}  // namespace cast
