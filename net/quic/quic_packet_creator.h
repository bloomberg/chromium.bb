// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Accumulates frames for the next packet until more frames no longer fit or
// it's time to create a packet from them.  Also provides packet creation of
// FEC packets based on previously created packets.

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

class QuicRandom;

class NET_EXPORT_PRIVATE QuicPacketCreator : public QuicFecBuilderInterface {
 public:
  // Options for controlling how packets are created.
  struct Options {
    Options()
        : max_packet_length(kMaxPacketSize),
          random_reorder(false),
          max_packets_per_fec_group(0) {
    }

    size_t max_packet_length;
    bool random_reorder;   // Inefficient: rewrite if used at scale.
    // 0 indicates fec is disabled.
    size_t max_packets_per_fec_group;
  };

  // QuicRandom* required for packet entropy.
  QuicPacketCreator(QuicGuid guid,
                    QuicFramer* framer,
                    QuicRandom* random_generator);

  virtual ~QuicPacketCreator();

  // QuicFecBuilderInterface
  virtual void OnBuiltFecProtectedPayload(const QuicPacketHeader& header,
                                          base::StringPiece payload) OVERRIDE;

  // Checks if it's time to send an FEC packet.  |force_close| forces this to
  // return true if an fec group is open.
  bool ShouldSendFec(bool force_close) const;

  // Starts a new FEC group with the next serialized packet, if FEC is enabled
  // and there is not already an FEC group open.
  void MaybeStartFEC();

  // The overhead the framing will add for a packet with num_frames frames.
  static size_t StreamFramePacketOverhead(int num_frames, bool include_version);

  bool HasRoomForStreamFrame() const;

  // Converts a raw payload to a frame which fits into the currently open
  // packet if there is one.  Returns the number of bytes consumed from data.
  // If data is empty and fin is true, the expected behavior is to consume the
  // fin but return 0.
  size_t CreateStreamFrame(QuicStreamId id,
                           base::StringPiece data,
                           QuicStreamOffset offset,
                           bool fin,
                           QuicFrame* frame);

  // Serializes all frames into a single packet. All frames must fit into a
  // single packet. Also, sets the entropy hash of the serialized packet to a
  // random bool and returns that value as a member of SerializedPacket.
  // Never returns a RetransmittableFrames in SerializedPacket.
  SerializedPacket SerializeAllFrames(const QuicFrames& frames);

  // Returns true if there are frames pending to be serialized.
  bool HasPendingFrames();

  // Returns the number of bytes which are free to frames in the current packet.
  size_t BytesFree() const;

  // Adds |frame| to the packet creator's list of frames to be serialized.
  // Returns false if the frame doesn't fit into the current packet.
  bool AddSavedFrame(const QuicFrame& frame);

  // Serializes all frames which have been added and adds any which should be
  // retransmitted to |retransmittable_frames| if it's not NULL. All frames must
  // fit into a single packet. Sets the entropy hash of the serialized
  // packet to a random bool and returns that value as a member of
  // SerializedPacket. Also, sets |serialized_frames| in the SerializedPacket
  // to the corresponding RetransmittableFrames if any frames are to be
  // retransmitted.
  SerializedPacket SerializePacket();

  // Packetize FEC data. All frames must fit into a single packet. Also, sets
  // the entropy hash of the serialized packet to a random bool and returns
  // that value as a member of SerializedPacket.
  SerializedPacket SerializeFec();

  // Creates a packet with connection close frame. Caller owns the created
  // packet. Also, sets the entropy hash of the serialized packet to a random
  // bool and returns that value as a member of SerializedPacket.
  SerializedPacket SerializeConnectionClose(
      QuicConnectionCloseFrame* close_frame);

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
  static bool ShouldRetransmit(const QuicFrame& frame);

  void FillPacketHeader(QuicFecGroupNumber fec_group,
                        bool fec_flag,
                        bool fec_entropy_flag,
                        QuicPacketHeader* header);

  // Allows a frame to be added without creating retransmittable frames.
  // Particularly useful for retransmits using SerializeAllFrames().
  bool AddFrame(const QuicFrame& frame, bool save_retransmittable_frames);

  Options options_;
  QuicGuid guid_;
  QuicFramer* framer_;
  QuicRandom* random_generator_;
  QuicPacketSequenceNumber sequence_number_;
  QuicFecGroupNumber fec_group_number_;
  scoped_ptr<QuicFecGroup> fec_group_;
  size_t packet_size_;
  QuicFrames queued_frames_;
  scoped_ptr<RetransmittableFrames> queued_retransmittable_frames_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacketCreator);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_CREATOR_H_
