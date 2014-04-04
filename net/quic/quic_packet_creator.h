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
#include "base/strings/string_piece.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {
namespace test {
class QuicPacketCreatorPeer;
}

class QuicAckNotifier;
class QuicRandom;
class QuicRandomBoolSource;

class NET_EXPORT_PRIVATE QuicPacketCreator : public QuicFecBuilderInterface {
 public:
  // Options for controlling how packets are created.
  struct Options {
    Options()
        : max_packet_length(kDefaultMaxPacketSize),
          max_packets_per_fec_group(0),
          send_connection_id_length(PACKET_8BYTE_CONNECTION_ID),
          send_sequence_number_length(PACKET_1BYTE_SEQUENCE_NUMBER) {}

    size_t max_packet_length;
    // 0 indicates fec is disabled.
    size_t max_packets_per_fec_group;
    // Length of connection_id to send over the wire.
    QuicConnectionIdLength send_connection_id_length;
    QuicSequenceNumberLength send_sequence_number_length;
  };

  // QuicRandom* required for packet entropy.
  QuicPacketCreator(QuicConnectionId connection_id,
                    QuicFramer* framer,
                    QuicRandom* random_generator,
                    bool is_server);

  virtual ~QuicPacketCreator();

  // QuicFecBuilderInterface
  virtual void OnBuiltFecProtectedPayload(const QuicPacketHeader& header,
                                          base::StringPiece payload) OVERRIDE;

  // Checks if it's time to send an FEC packet.  |force_close| forces this to
  // return true if an fec group is open.
  bool ShouldSendFec(bool force_close) const;

  // Makes the framer not serialize the protocol version in sent packets.
  void StopSendingVersion();

  // Update the sequence number length to use in future packets as soon as it
  // can be safely changed.
  void UpdateSequenceNumberLength(
      QuicPacketSequenceNumber least_packet_awaited_by_peer,
      QuicByteCount bytes_per_second);

  // The overhead the framing will add for a packet with one frame.
  static size_t StreamFramePacketOverhead(
      QuicVersion version,
      QuicConnectionIdLength connection_id_length,
      bool include_version,
      QuicSequenceNumberLength sequence_number_length,
      InFecGroup is_in_fec_group);

  bool HasRoomForStreamFrame(QuicStreamId id, QuicStreamOffset offset) const;

  // Converts a raw payload to a frame which fits into the currently open
  // packet if there is one.  Returns the number of bytes consumed from data.
  // If data is empty and fin is true, the expected behavior is to consume the
  // fin but return 0.
  size_t CreateStreamFrame(QuicStreamId id,
                           const IOVector& data,
                           QuicStreamOffset offset,
                           bool fin,
                           QuicFrame* frame);

  // As above, but keeps track of an QuicAckNotifier that should be called when
  // the packet that contains this stream frame is ACKed.
  // The |notifier| is not owned by the QuicPacketGenerator and must outlive the
  // generated packet.
  size_t CreateStreamFrameWithNotifier(QuicStreamId id,
                                       const IOVector& data,
                                       QuicStreamOffset offset,
                                       bool fin,
                                       QuicAckNotifier* notifier,
                                       QuicFrame* frame);

  // Serializes all frames into a single packet. All frames must fit into a
  // single packet. Also, sets the entropy hash of the serialized packet to a
  // random bool and returns that value as a member of SerializedPacket.
  // Never returns a RetransmittableFrames in SerializedPacket.
  SerializedPacket SerializeAllFrames(const QuicFrames& frames);

  // Re-serializes frames with the original packet's sequence number length.
  // Used for retransmitting packets to ensure they aren't too long.
  SerializedPacket ReserializeAllFrames(
      const QuicFrames& frames, QuicSequenceNumberLength original_length);

  // Returns true if there are frames pending to be serialized.
  bool HasPendingFrames();

  // Returns the number of bytes which are available to be used by additional
  // frames in the packet.  Since stream frames are slightly smaller when they
  // are the last frame in a packet, this method will return a different
  // value than max_packet_size - PacketSize(), in this case.
  size_t BytesFree() const;

  // Returns the number of bytes that the packet will expand by if a new frame
  // is added to the packet. If the last frame was a stream frame, it will
  // expand slightly when a new frame is added, and this method returns the
  // amount of expected expansion. If the packet is in an FEC group, no
  // expansion happens and this method always returns zero.
  size_t ExpansionOnNewFrame() const;

  // Returns the number of bytes in the current packet, including the header,
  // if serialized with the current frames.  Adding a frame to the packet
  // may change the serialized length of existing frames, as per the comment
  // in BytesFree.
  size_t PacketSize() const;

  // TODO(jri): AddSavedFrame calls AddFrame, which only saves the frame
  // if it is a stream frame, not other types of frames. Fix this API;
  // add a AddNonSavedFrame method.
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

  // Creates a version negotiation packet which supports |supported_versions|.
  // Caller owns the created  packet. Also, sets the entropy hash of the
  // serialized packet to a random bool and returns that value as a member of
  // SerializedPacket.
  QuicEncryptedPacket* SerializeVersionNegotiationPacket(
      const QuicVersionVector& supported_versions);

  // Sequence number of the last created packet, or 0 if no packets have been
  // created.
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
  friend class test::QuicPacketCreatorPeer;

  static bool ShouldRetransmit(const QuicFrame& frame);

  // Starts a new FEC group with the next serialized packet, if FEC is enabled
  // and there is not already an FEC group open.
  InFecGroup MaybeStartFEC();

  void FillPacketHeader(QuicFecGroupNumber fec_group,
                        bool fec_flag,
                        QuicPacketHeader* header);

  // Allows a frame to be added without creating retransmittable frames.
  // Particularly useful for retransmits using SerializeAllFrames().
  bool AddFrame(const QuicFrame& frame, bool save_retransmittable_frames);

  // Adds a padding frame to the current packet only if the current packet
  // contains a handshake message, and there is sufficient room to fit a
  // padding frame.
  void MaybeAddPadding();

  Options options_;
  QuicConnectionId connection_id_;
  QuicFramer* framer_;
  scoped_ptr<QuicRandomBoolSource> random_bool_source_;
  QuicPacketSequenceNumber sequence_number_;
  QuicFecGroupNumber fec_group_number_;
  scoped_ptr<QuicFecGroup> fec_group_;
  // bool to keep track if this packet creator is being used the server.
  bool is_server_;
  // Controls whether protocol version should be included while serializing the
  // packet.
  bool send_version_in_packet_;
  // The sequence number length for the current packet and the current FEC group
  // if FEC is enabled.
  // Mutable so PacketSize() can adjust it when the packet is empty.
  mutable QuicSequenceNumberLength sequence_number_length_;
  // packet_size_ is mutable because it's just a cache of the current size.
  // packet_size should never be read directly, use PacketSize() instead.
  mutable size_t packet_size_;
  QuicFrames queued_frames_;
  scoped_ptr<RetransmittableFrames> queued_retransmittable_frames_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacketCreator);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_CREATOR_H_
