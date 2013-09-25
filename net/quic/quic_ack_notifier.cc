// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier.h"

#include <set>

#include "base/logging.h"
#include "base/stl_util.h"

namespace net {

QuicAckNotifier::DelegateInterface::DelegateInterface() {}

QuicAckNotifier::DelegateInterface::~DelegateInterface() {}

QuicAckNotifier::QuicAckNotifier(DelegateInterface* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

QuicAckNotifier::~QuicAckNotifier() {}

void QuicAckNotifier::AddSequenceNumber(
    const QuicPacketSequenceNumber& sequence_number) {
  sequence_numbers_.insert(sequence_number);
}

void QuicAckNotifier::AddSequenceNumbers(
    const SequenceNumberSet& sequence_numbers) {
  for (SequenceNumberSet::const_iterator it = sequence_numbers.begin();
       it != sequence_numbers.end(); ++it) {
    AddSequenceNumber(*it);
  }
}

bool QuicAckNotifier::OnAck(QuicPacketSequenceNumber sequence_number) {
  DCHECK(ContainsKey(sequence_numbers_, sequence_number));
  sequence_numbers_.erase(sequence_number);
  if (IsEmpty()) {
    // We have seen all the sequence numbers we were waiting for, trigger
    // callback notification.
    delegate_->OnAckNotification();
    return true;
  }
  return false;
}

void QuicAckNotifier::UpdateSequenceNumber(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  sequence_numbers_.erase(old_sequence_number);
  sequence_numbers_.insert(new_sequence_number);
}

};  // namespace net
