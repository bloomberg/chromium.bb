// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker.h"

#include <algorithm>

namespace {
IPC::AttachmentBroker* g_attachment_broker = nullptr;
}

namespace IPC {

// static
void AttachmentBroker::SetGlobal(AttachmentBroker* broker) {
  CHECK(!g_attachment_broker || !broker)
      << "Global attachment broker address: " << broker
      << ". New attachment broker address: " << broker;
  g_attachment_broker = broker;
}

// static
AttachmentBroker* AttachmentBroker::GetGlobal() {
  return g_attachment_broker;
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
AttachmentBroker::AttachmentBroker() : port_provider_(nullptr) {}
#else
AttachmentBroker::AttachmentBroker() {}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

AttachmentBroker::~AttachmentBroker() {}

bool AttachmentBroker::GetAttachmentWithId(
    BrokerableAttachment::AttachmentId id,
    scoped_refptr<BrokerableAttachment>* out_attachment) {
  for (AttachmentVector::iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    if ((*it)->GetIdentifier() == id) {
      *out_attachment = *it;
      attachments_.erase(it);
      return true;
    }
  }
  return false;
}

void AttachmentBroker::AddObserver(AttachmentBroker::Observer* observer) {
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it == observers_.end())
    observers_.push_back(observer);
}

void AttachmentBroker::RemoveObserver(AttachmentBroker::Observer* observer) {
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it != observers_.end())
    observers_.erase(it);
}

void AttachmentBroker::RegisterCommunicationChannel(Endpoint* endpoint) {
  NOTREACHED();
}

void AttachmentBroker::DeregisterCommunicationChannel(Endpoint* endpoint) {
  NOTREACHED();
}

void AttachmentBroker::HandleReceivedAttachment(
    const scoped_refptr<BrokerableAttachment>& attachment) {
  attachments_.push_back(attachment);
  NotifyObservers(attachment->GetIdentifier());
}

void AttachmentBroker::NotifyObservers(
    const BrokerableAttachment::AttachmentId& id) {
  // Make a copy of observers_ to avoid mutations during iteration.
  std::vector<Observer*> observers = observers_;
  for (Observer* observer : observers) {
    observer->ReceivedBrokerableAttachmentWithId(id);
  }
}

}  // namespace IPC
