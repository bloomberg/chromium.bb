// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker.h"

#include <algorithm>

namespace IPC {

AttachmentBroker::AttachmentBroker() {
}
AttachmentBroker::~AttachmentBroker() {
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

void AttachmentBroker::NotifyObservers(
    const BrokerableAttachment::AttachmentId& id) {
  // Make a copy of observers_ to avoid mutations during iteration.
  std::vector<Observer*> observers = observers_;
  for (Observer* observer : observers) {
    observer->ReceivedBrokerableAttachmentWithId(id);
  }
}

}  // namespace IPC
