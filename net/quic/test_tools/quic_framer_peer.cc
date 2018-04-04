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
bool QuicFramerPeer::AppendIetfAckFrame(QuicFramer* framer,
                                        const QuicAckFrame& frame,
                                        QuicDataWriter* writer) {
  return framer->AppendIetfAckFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendIetfConnectionCloseFrame(
    QuicFramer* framer,
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfConnectionCloseFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendIetfApplicationCloseFrame(
    QuicFramer* framer,
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfApplicationCloseFrame(frame, writer);
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
bool QuicFramerPeer::AppendIetfPaddingFrame(QuicFramer* framer,
                                            const QuicPaddingFrame& frame,
                                            QuicDataWriter* writer) {
  return framer->AppendIetfPaddingFrame(frame, writer);
}

// static
void QuicFramerPeer::ProcessIetfPaddingFrame(QuicFramer* framer,
                                             QuicDataReader* reader,
                                             QuicPaddingFrame* frame) {
  framer->ProcessIetfPaddingFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessIetfPathChallengeFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicPathChallengeFrame* frame) {
  return framer->ProcessIetfPathChallengeFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessIetfPathResponseFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicPathResponseFrame* frame) {
  return framer->ProcessIetfPathResponseFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendIetfPathChallengeFrame(
    QuicFramer* framer,
    const QuicPathChallengeFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfPathChallengeFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendIetfPathResponseFrame(
    QuicFramer* framer,
    const QuicPathResponseFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfPathResponseFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendIetfResetStreamFrame(QuicFramer* framer,
                                                const QuicRstStreamFrame& frame,
                                                QuicDataWriter* writer) {
  return framer->AppendIetfResetStreamFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfResetStreamFrame(QuicFramer* framer,
                                                 QuicDataReader* reader,
                                                 QuicRstStreamFrame* frame) {
  return framer->ProcessIetfResetStreamFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessIetfStopSendingFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicStopSendingFrame* stop_sending_frame) {
  return framer->ProcessIetfStopSendingFrame(reader, stop_sending_frame);
}

// static
bool QuicFramerPeer::AppendIetfStopSendingFrame(
    QuicFramer* framer,
    const QuicStopSendingFrame& stop_sending_frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfStopSendingFrame(stop_sending_frame, writer);
}

// Append/consume IETF-Format MAX_DATA and MAX_STREAM_DATA frames
// static
// static
bool QuicFramerPeer::AppendIetfMaxDataFrame(QuicFramer* framer,
                                            const QuicWindowUpdateFrame& frame,
                                            QuicDataWriter* writer) {
  return framer->AppendIetfMaxDataFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendIetfMaxStreamDataFrame(
    QuicFramer* framer,
    const QuicWindowUpdateFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfMaxStreamDataFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfMaxDataFrame(QuicFramer* framer,
                                             QuicDataReader* reader,
                                             QuicWindowUpdateFrame* frame) {
  return framer->ProcessIetfMaxDataFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessIetfMaxStreamDataFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicWindowUpdateFrame* frame) {
  return framer->ProcessIetfMaxStreamDataFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendIetfMaxStreamIdFrame(
    QuicFramer* framer,
    const QuicIetfMaxStreamIdFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfMaxStreamIdFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfMaxStreamIdFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicIetfMaxStreamIdFrame* frame) {
  return framer->ProcessIetfMaxStreamIdFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendIetfBlockedFrame(QuicFramer* framer,
                                            const QuicIetfBlockedFrame& frame,
                                            QuicDataWriter* writer) {
  return framer->AppendIetfBlockedFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfBlockedFrame(QuicFramer* framer,
                                             QuicDataReader* reader,
                                             QuicIetfBlockedFrame* frame) {
  return framer->ProcessIetfBlockedFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendIetfStreamBlockedFrame(
    QuicFramer* framer,
    const QuicWindowUpdateFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfStreamBlockedFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfStreamBlockedFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicWindowUpdateFrame* frame) {
  return framer->ProcessIetfStreamBlockedFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendIetfStreamIdBlockedFrame(
    QuicFramer* framer,
    const QuicIetfStreamIdBlockedFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfStreamIdBlockedFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfStreamIdBlockedFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicIetfStreamIdBlockedFrame* frame) {
  return framer->ProcessIetfStreamIdBlockedFrame(reader, frame);
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
