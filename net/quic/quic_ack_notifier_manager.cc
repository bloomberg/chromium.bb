// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier_manager.h"

#include <stddef.h>
#include <list>
#include <map>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_protocol.h"

namespace net {

AckNotifierManager::AckNotifierManager() {}

AckNotifierManager::~AckNotifierManager() {
  STLDeleteElements(&ack_notifiers_);
}

void AckNotifierManager::OnIncomingAck(const SequenceNumberSet& acked_packets) {
  // Inform all the registered AckNotifiers of the new ACKs.
  for (SequenceNumberSet::const_iterator seq_it = acked_packets.begin();
       seq_it != acked_packets.end(); ++seq_it) {
    AckNotifierMap::iterator map_it = ack_notifier_map_.find(*seq_it);
    if (map_it == ack_notifier_map_.end()) {
      // No AckNotifier is interested in this sequence number.
      continue;
    }

    // One or more AckNotifiers are registered as interested in this sequence
    // number. Iterate through them and call OnAck on each.
    for (AckNotifierSet::iterator set_it = map_it->second.begin();
         set_it != map_it->second.end(); ++set_it) {
      (*set_it)->OnAck(*seq_it);
    }

    // Remove the sequence number from the map as we have notified all the
    // registered AckNotifiers, and we won't see it again.
    ack_notifier_map_.erase(map_it);
  }

  // Clear up any empty AckNotifiers
  AckNotifierSet::iterator it = ack_notifiers_.begin();
  while (it != ack_notifiers_.end()) {
    if ((*it)->IsEmpty()) {
      // The QuicAckNotifier has seen all the ACKs it was interested in, and
      // has triggered its callback. No more use for it.
      delete *it;
      ack_notifiers_.erase(it++);
    } else {
      ++it;
    }
  }
}

void AckNotifierManager::UpdateSequenceNumber(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  AckNotifierMap::iterator map_it = ack_notifier_map_.find(old_sequence_number);
  if (map_it != ack_notifier_map_.end()) {
    // We will add an entry to the map for the new sequence number, and move
    // the
    // list of AckNotifiers over.
    AckNotifierSet new_set;
    for (AckNotifierSet::iterator notifier_it = map_it->second.begin();
         notifier_it != map_it->second.end(); ++notifier_it) {
      (*notifier_it)
          ->UpdateSequenceNumber(old_sequence_number, new_sequence_number);
      new_set.insert(*notifier_it);
    }
    ack_notifier_map_[new_sequence_number] = new_set;
    ack_notifier_map_.erase(map_it);
  }
}

void AckNotifierManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  // Run through all the frames and if any of them are stream frames and have
  // an AckNotifier registered, then inform the AckNotifier that it should be
  // interested in this packet's sequence number.

  RetransmittableFrames* frames = serialized_packet.retransmittable_frames;

  // AckNotifiers can only be attached to retransmittable frames.
  if (!frames) {
    return;
  }

  for (QuicFrames::const_iterator it = frames->frames().begin();
       it != frames->frames().end(); ++it) {
    if (it->type == STREAM_FRAME && it->stream_frame->notifier != NULL) {
      // The AckNotifier needs to know it is tracking this packet's sequence
      // number.
      it->stream_frame->notifier->AddSequenceNumber(
          serialized_packet.sequence_number);

      // Add the AckNotifier to our set of AckNotifiers.
      ack_notifiers_.insert(it->stream_frame->notifier);

      // Update the mapping in the other direction, from sequence
      // number to AckNotifier.
      ack_notifier_map_[serialized_packet.sequence_number]
          .insert(it->stream_frame->notifier);
    }
  }
}

}  // namespace net
