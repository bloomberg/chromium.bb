// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier.h"

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

bool QuicAckNotifier::OnAck(SequenceNumberSet sequence_numbers) {
  // If the set of sequence numbers we are tracking is empty then this
  // QuicAckNotifier should have already been deleted.
  DCHECK(!sequence_numbers_.empty());

  for (SequenceNumberSet::iterator it = sequence_numbers.begin();
       it != sequence_numbers.end(); ++it) {
    sequence_numbers_.erase(*it);
    if (sequence_numbers_.empty()) {
      delegate_->OnAckNotification();
      return true;
    }
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
