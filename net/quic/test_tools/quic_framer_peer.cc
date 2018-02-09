// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_framer_peer.h"

#include "net/quic/core/quic_framer.h"
#include "net/quic/core/quic_packets.h"
#include "net/quic/platform/api/quic_map_util.h"

using std::string;

namespace net {
namespace test {

// static
QuicPacketNumber QuicFramerPeer::CalculatePacketNumberFromWire(
    QuicFramer* framer,
    QuicPacketNumberLength packet_number_length,
    QuicPacketNumber last_packet_number,
    QuicPacketNumber packet_number) {
  return framer->CalculatePacketNumberFromWire(
      packet_number_length, last_packet_number, packet_number);
}

// static
void QuicFramerPeer::SetLastSerializedConnectionId(
    QuicFramer* framer,
    QuicConnectionId connection_id) {
  framer->last_serialized_connection_id_ = connection_id;
}

// static
void QuicFramerPeer::SetLargestPacketNumber(QuicFramer* framer,
                                            QuicPacketNumber packet_number) {
  framer->largest_packet_number_ = packet_number;
}

// static
void QuicFramerPeer::SetPerspective(QuicFramer* framer,
                                    Perspective perspective) {
  framer->perspective_ = perspective;
}

// static
bool QuicFramerPeer::ProcessIetfStreamFrame(QuicFramer* framer,
                                            QuicDataReader* reader,
                                            uint8_t frame_type,
                                            QuicStreamFrame* frame) {
  return framer->ProcessIetfStreamFrame(reader, frame_type, frame);
}
// static
bool QuicFramerPeer::AppendIetfStreamFrame(QuicFramer* framer,
                                           const QuicStreamFrame& frame,
                                           bool last_frame_in_packet,
                                           QuicDataWriter* writer) {
  return framer->AppendIetfStreamFrame(frame, last_frame_in_packet, writer);
}
// static
bool QuicFramerPeer::ProcessIetfAckFrame(QuicFramer* framer,
                                         QuicDataReader* reader,
                                         uint8_t frame_type,
                                         QuicAckFrame* ack_frame) {
  return framer->ProcessIetfAckFrame(reader, frame_type, ack_frame);
}
// static
bool QuicFramerPeer::AppendIetfAckFrameAndTypeByte(QuicFramer* framer,
                                                   const QuicAckFrame& frame,
                                                   QuicDataWriter* writer) {
  return framer->AppendIetfAckFrameAndTypeByte(frame, writer);
}

// static
bool QuicFramerPeer::AppendIetfConnectionCloseFrame(
    QuicFramer* framer,
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfConnectionCloseFrame(frame, writer);
}
// static
bool QuicFramerPeer::AppendIetfConnectionCloseFrame(
    QuicFramer* framer,
    const QuicIetfTransportErrorCodes code,
    const string& phrase,
    QuicDataWriter* writer) {
  return framer->AppendIetfConnectionCloseFrame(code, phrase, writer);
}
// static
bool QuicFramerPeer::AppendIetfApplicationCloseFrame(
    QuicFramer* framer,
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfApplicationCloseFrame(frame, writer);
}
// static
bool QuicFramerPeer::AppendIetfApplicationCloseFrame(QuicFramer* framer,
                                                     const uint16_t code,
                                                     const string& phrase,
                                                     QuicDataWriter* writer) {
  return framer->AppendIetfApplicationCloseFrame(code, phrase, writer);
}
// static
bool QuicFramerPeer::ProcessIetfConnectionCloseFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    const uint8_t frame_type,
    QuicConnectionCloseFrame* frame) {
  return framer->ProcessIetfConnectionCloseFrame(reader, frame_type, frame);
}
// static
bool QuicFramerPeer::ProcessIetfApplicationCloseFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    const uint8_t frame_type,
    QuicConnectionCloseFrame* frame) {
  return framer->ProcessIetfApplicationCloseFrame(reader, frame_type, frame);
}

// static
void QuicFramerPeer::SwapCrypters(QuicFramer* framer1, QuicFramer* framer2) {
  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; i++) {
    framer1->encrypter_[i].swap(framer2->encrypter_[i]);
  }
  framer1->decrypter_.swap(framer2->decrypter_);
  framer1->alternative_decrypter_.swap(framer2->alternative_decrypter_);

  EncryptionLevel framer2_level = framer2->decrypter_level_;
  framer2->decrypter_level_ = framer1->decrypter_level_;
  framer1->decrypter_level_ = framer2_level;
  framer2_level = framer2->alternative_decrypter_level_;
  framer2->alternative_decrypter_level_ = framer1->alternative_decrypter_level_;
  framer1->alternative_decrypter_level_ = framer2_level;

  const bool framer2_latch = framer2->alternative_decrypter_latch_;
  framer2->alternative_decrypter_latch_ = framer1->alternative_decrypter_latch_;
  framer1->alternative_decrypter_latch_ = framer2_latch;
}

// static
QuicEncrypter* QuicFramerPeer::GetEncrypter(QuicFramer* framer,
                                            EncryptionLevel level) {
  return framer->encrypter_[level].get();
}

}  // namespace test
}  // namespace net
