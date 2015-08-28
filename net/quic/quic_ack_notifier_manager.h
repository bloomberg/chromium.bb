// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_
#define NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_

#include <list>
#include <map>

#include "base/containers/hash_tables.h"
#include "net/quic/quic_protocol.h"

namespace net {

class QuicAckNotifier;

namespace test {
class AckNotifierManagerPeer;
}

// The AckNotifierManager is used by the QuicSentPacketManager to keep track of
// all the AckNotifiers currently active. It owns the AckNotifiers which it gets
// from the serialized packets passed into OnSerializedPacket. It maintains both
// a set of AckNotifiers and a map from packet number to AckNotifier the sake
// of efficiency - we can quickly check the map to see if any AckNotifiers are
// interested in a given packet number.
class NET_EXPORT_PRIVATE AckNotifierManager {
 public:
  AckNotifierManager();
  virtual ~AckNotifierManager();

  // Called when the connection receives a new AckFrame.  If |packet_number|
  // exists in ack_notifier_map_ then the corresponding AckNotifiers will have
  // their OnAck method called.
  void OnPacketAcked(QuicPacketNumber packet_number,
                     QuicTime::Delta delta_largest_observed);

  // If a packet has been retransmitted with a new packet number, then this
  // will be called. It updates the mapping in ack_notifier_map_, and also
  // updates the internal set of packet numbers in each matching AckNotifier.
  void OnPacketRetransmitted(QuicPacketNumber old_packet_number,
                             QuicPacketNumber new_packet_number,
                             int packet_payload_size);

  // This is called after a packet has been serialized, is ready to be sent, and
  // contains retransmittable frames (which may have associated AckNotifiers).
  // If any of the retransmittable frames included in |serialized_packet| have
  // AckNotifiers registered, then add them to our internal map and additionally
  // inform the AckNotifier of the packet number which it should track.
  void OnSerializedPacket(const SerializedPacket& serialized_packet);

  // This method is invoked when a packet is removed from the list of unacked
  // packets, and it is no longer necessary to keep track of the notifier.
  void OnPacketRemoved(QuicPacketNumber packet_number);

 private:
  friend class test::AckNotifierManagerPeer;

  typedef std::list<QuicAckNotifier*> AckNotifierList;
  // TODO(ianswett): Further improvement may come from changing this to a deque.
  typedef base::hash_map<QuicPacketNumber, AckNotifierList> AckNotifierMap;

  // Maps from packet number to the AckNotifiers which are registered
  // for that packet number. On receipt of an ACK for a given sequence
  // number, call OnAck for all mapped AckNotifiers.
  // When the last reference is removed from the map, the notifier is deleted.
  AckNotifierMap ack_notifier_map_;

  DISALLOW_COPY_AND_ASSIGN(AckNotifierManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_
