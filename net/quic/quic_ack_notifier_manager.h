// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_
#define NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_

#include <list>
#include <map>
#include <set>

#include "net/quic/quic_protocol.h"

namespace net {

class QuicAckNotifier;

// The AckNotifierManager is used by the QuicConnection to keep track of all the
// AckNotifiers currently active. It owns the AckNotifiers which it gets from
// the serialized packets passed into OnSerializedPacket. It maintains both a
// list of AckNotifiers and a map from sequence number to AckNotifier the sake
// of efficiency - we can quickly check the map to see if any AckNotifiers are
// interested in a given sequence number.

class NET_EXPORT_PRIVATE AckNotifierManager {
 public:
  AckNotifierManager();
  virtual ~AckNotifierManager();

  // Called from QuicConnection when it receives a new AckFrame. For each packet
  // in |acked_packets|, if the packet sequence number exists in
  // ack_notifier_map_ then the corresponding AckNotifiers will have their OnAck
  // method called.
  void OnIncomingAck(const SequenceNumberSet& acked_packets);

  // If a packet has been retransmitted with a new sequence number, then this
  // will be called. It updates the mapping in ack_notifier_map_, and also
  // updates the internal list of sequence numbers in each matching AckNotifier.
  void UpdateSequenceNumber(QuicPacketSequenceNumber old_sequence_number,
                            QuicPacketSequenceNumber new_sequence_number);

  // This is called after a packet has been serialized and is ready to be sent.
  // If any of the frames in |serialized_packet| have AckNotifiers registered,
  // then add them to our internal map and additionally inform the AckNotifier
  // of the sequence number which it should track.
  void OnSerializedPacket(const SerializedPacket& serialized_packet);

  // Called from QuicConnection when data is sent which the sender would like to
  // be notified on receipt of all ACKs. Adds the |notifier| to our map.
  void AddAckNotifier(QuicAckNotifier* notifier);

 private:
  typedef std::list<QuicAckNotifier*> AckNotifierList;
  typedef std::set<QuicAckNotifier*> AckNotifierSet;
  typedef std::map<QuicPacketSequenceNumber, AckNotifierSet> AckNotifierMap;

  // On every ACK frame received by this connection, all the ack_notifiers_ will
  // be told which sequeunce numbers were ACKed.
  // Once a given QuicAckNotifier has seen all the sequence numbers it is
  // interested in, it will be deleted, and removed from this list.
  // Owns the AckNotifiers in this list.
  AckNotifierList ack_notifiers_;

  // Maps from sequence number to the AckNotifiers which are registered
  // for that sequence number. On receipt of an ACK for a given sequence
  // number, call OnAck for all mapped AckNotifiers.
  // Does not own the AckNotifiers.
  std::map<QuicPacketSequenceNumber, AckNotifierSet> ack_notifier_map_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_
