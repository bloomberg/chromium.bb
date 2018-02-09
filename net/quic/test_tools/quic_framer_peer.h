// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_

#include "base/macros.h"
#include "net/quic/core/crypto/quic_encrypter.h"
#include "net/quic/core/quic_framer.h"
#include "net/quic/core/quic_packets.h"

namespace net {

namespace test {

class QuicFramerPeer {
 public:
  static QuicPacketNumber CalculatePacketNumberFromWire(
      QuicFramer* framer,
      QuicPacketNumberLength packet_number_length,
      QuicPacketNumber last_packet_number,
      QuicPacketNumber packet_number);
  static void SetLastSerializedConnectionId(QuicFramer* framer,
                                            QuicConnectionId connection_id);
  static void SetLargestPacketNumber(QuicFramer* framer,
                                     QuicPacketNumber packet_number);
  static void SetPerspective(QuicFramer* framer, Perspective perspective);

  // SwapCrypters exchanges the state of the crypters of |framer1| with
  // |framer2|.
  static void SwapCrypters(QuicFramer* framer1, QuicFramer* framer2);

  static QuicEncrypter* GetEncrypter(QuicFramer* framer, EncryptionLevel level);

  // IETF defined frame append/process methods.
  static bool ProcessIetfStreamFrame(QuicFramer* framer,
                                     QuicDataReader* reader,
                                     uint8_t frame_type,
                                     QuicStreamFrame* frame);
  static bool AppendIetfStreamFrame(QuicFramer* framer,
                                    const QuicStreamFrame& frame,
                                    bool last_frame_in_packet,
                                    QuicDataWriter* writer);

  static bool AppendIetfConnectionCloseFrame(
      QuicFramer* framer,
      const QuicConnectionCloseFrame& frame,
      QuicDataWriter* writer);
  static bool AppendIetfConnectionCloseFrame(
      QuicFramer* framer,
      const QuicIetfTransportErrorCodes code,
      const std::string& phrase,
      QuicDataWriter* writer);
  static bool AppendIetfApplicationCloseFrame(
      QuicFramer* framer,
      const QuicConnectionCloseFrame& frame,
      QuicDataWriter* writer);
  static bool AppendIetfApplicationCloseFrame(QuicFramer* framer,
                                              const uint16_t code,
                                              const std::string& phrase,
                                              QuicDataWriter* writer);
  static bool ProcessIetfConnectionCloseFrame(QuicFramer* framer,
                                              QuicDataReader* reader,
                                              const uint8_t frame_type,
                                              QuicConnectionCloseFrame* frame);
  static bool ProcessIetfApplicationCloseFrame(QuicFramer* framer,
                                               QuicDataReader* reader,
                                               const uint8_t frame_type,
                                               QuicConnectionCloseFrame* frame);

  static bool ProcessIetfAckFrame(QuicFramer* framer,
                                  QuicDataReader* reader,
                                  uint8_t frame_type,
                                  QuicAckFrame* ack_frame);
  static bool AppendIetfAckFrameAndTypeByte(QuicFramer* framer,
                                            const QuicAckFrame& frame,
                                            QuicDataWriter* writer);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicFramerPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
