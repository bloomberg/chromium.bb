// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic packet creation.

#ifndef NET_QUIC_QUIC_PACKET_CREATOR_H_
#define NET_QUIC_QUIC_PACKET_CREATOR_H_

#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicPacketCreator : public QuicFecBuilderInterface {
 public:
  // Options for controlling how packets are created.
  struct Options {
    Options()
        : max_packet_length(kMaxPacketSize),
          random_reorder(false),
          max_packets_per_fec_group(0) {
    }

    // TODO(alyssar, rch) max frames/packet
    size_t max_packet_length;
    bool random_reorder;   // Inefficient: rewrite if used at scale.
    // 0 indicates fec is disabled.
    size_t max_packets_per_fec_group;
  };

  QuicPacketCreator(QuicGuid guid, QuicFramer* framer);

  virtual ~QuicPacketCreator();

  // QuicFecBuilderInterface
  virtual void OnBuiltFecProtectedPayload(const QuicPacketHeader& header,
                                          base::StringPiece payload) OVERRIDE;

  typedef std::pair<QuicPacketSequenceNumber, QuicPacket*> PacketPair;

  // Checks if it's time to send an FEC packet.  |force_close| forces this to
  // return true if an fec group is open.
  bool ShouldSendFec(bool force_close) const;

  // Starts a new FEC group with the next serialized packet, if FEC is enabled.
  void MaybeStartFEC();

  // Converts a raw payload to a frame.  Returns the number of bytes consumed
  // from data.  If data is empty and fin is true, the expected behavior is to
  // consume the fin but return 0.
  size_t CreateStreamFrame(QuicStreamId id,
                           base::StringPiece data,
                           QuicStreamOffset offset,
                           bool fin,
                           QuicFrames* frames);

  // Serializes all frames into a single packet.  All frames must fit into a
  // single packet.
  PacketPair SerializeAllFrames(const QuicFrames& frames);

  // Serializes as many non-fec frames as can fit into a single packet.
  // num_serialized is set to the number of frames serialized into the packet.
  PacketPair SerializeFrames(const QuicFrames& frames,
                             size_t* num_serialized);

  // Packetize FEC data.
  PacketPair SerializeFec();

  PacketPair CloseConnection(QuicConnectionCloseFrame* close_frame);

  QuicPacketSequenceNumber sequence_number() const {
    return sequence_number_;
  }

  void set_sequence_number(QuicPacketSequenceNumber s) {
    sequence_number_ = s;
  }

  Options* options() {
    return &options_;
  }

 private:
  void FillPacketHeader(QuicFecGroupNumber fec_group,
                        QuicPacketPrivateFlags flags,
                        QuicPacketHeader* header);

  Options options_;
  QuicGuid guid_;
  QuicFramer* framer_;
  QuicPacketSequenceNumber sequence_number_;
  QuicFecGroupNumber fec_group_number_;
  scoped_ptr<QuicFecGroup> fec_group_;

};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_CREATOR_H_
