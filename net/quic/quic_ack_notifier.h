// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_ACK_NOTIFIER_H_
#define NET_QUIC_QUIC_ACK_NOTIFIER_H_

#include "net/quic/quic_protocol.h"

namespace net {

// Used to register with a QuicConnection for notification once a set of packets
// have all been ACKed.
// The connection informs this class of newly ACKed sequence numbers, and once
// we have seen ACKs for all the sequence numbers we are interested in, we
// trigger a call to a provided Closure.
class NET_EXPORT_PRIVATE QuicAckNotifier {
 public:
  class NET_EXPORT_PRIVATE DelegateInterface {
   public:
    DelegateInterface();
    virtual ~DelegateInterface();
    virtual void OnAckNotification() = 0;
  };

  explicit QuicAckNotifier(DelegateInterface* delegate);
  virtual ~QuicAckNotifier();

  // Register a sequence number that this AckNotifier should be interested in.
  void AddSequenceNumber(const QuicPacketSequenceNumber& sequence_number);

  // Register a set of sequence numbers that this AckNotifier should be
  // interested in.
  void AddSequenceNumbers(const SequenceNumberSet& sequence_numbers);

  // Called by the QuicConnection on receipt of new ACK frame, with the sequence
  // number referenced by the ACK frame.
  // Deletes the matching sequence number from the stored set of sequence
  // numbers. If this set is now empty, call the stored delegate's
  // OnAckNotification method.
  //
  // Returns true if the provided sequence_number caused the delegate to be
  // called, false otherwise.
  bool OnAck(QuicPacketSequenceNumber sequence_number);

  bool IsEmpty() { return sequence_numbers_.empty(); }

  // If a packet is retransmitted by the connection it will be sent with a
  // different sequence number. Updates our internal set of sequence_numbers to
  // track the latest number.
  void UpdateSequenceNumber(QuicPacketSequenceNumber old_sequence_number,
                            QuicPacketSequenceNumber new_sequence_number);

 private:
  // The delegate's OnAckNotification() method will be called once we have been
  // notified of ACKs for all the sequence numbers we are tracking.
  // This is not owned by OnAckNotifier and must outlive it.
  DelegateInterface* delegate_;

  // Set of sequence numbers this notifier is waiting to hear about. The
  // delegate will not be called until this is an empty set.
  SequenceNumberSet sequence_numbers_;
};

};  // namespace net

#endif  // NET_QUIC_QUIC_ACK_NOTIFIER_H_
