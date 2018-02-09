// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// gunit tests for the IETF-format framer --- generally does a simple test
// for each framer; we generate the template object (eg
// QuicIetfStreamFrame) with the correct stuff in it, ask that a frame
// be serialized (call AppendIetf<mumble>) then deserialized (call
// ProcessIetf<mumble>) and then check that the gazintas and gazoutas
// are the same.
//
// We do minimal checking of the serialized frame
//
// We do look at various different values (resulting in different
// length varints, etc)

#include "net/quic/core/quic_framer.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/quic/core/crypto/null_decrypter.h"
#include "net/quic/core/crypto/null_encrypter.h"
#include "net/quic/core/crypto/quic_decrypter.h"
#include "net/quic/core/crypto/quic_encrypter.h"
#include "net/quic/core/quic_data_reader.h"
#include "net/quic/core/quic_data_writer.h"
#include "net/quic/core/quic_packets.h"
#include "net/quic/core/quic_utils.h"
#include "net/quic/platform/api/quic_arraysize.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_logging.h"
#include "net/quic/platform/api/quic_ptr_util.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_data_producer.h"

using std::string;

namespace net {
namespace test {
namespace {

const size_t kNormalPacketBufferSize = 1400;
// several different stream ids, should be encoded
// in 8, 4, 2, and 1 byte, respectively. Last one
// checks that value==0 works.
const QuicIetfStreamId kStreamId8 = UINT64_C(0x3EDCBA9876543210);
const QuicIetfStreamId kStreamId4 = UINT64_C(0x36543210);
const QuicIetfStreamId kStreamId2 = UINT64_C(0x3210);
const QuicIetfStreamId kStreamId1 = UINT64_C(0x10);
const QuicIetfStreamId kStreamId0 = UINT64_C(0x00);

// Ditto for the offsets.
const QuicIetfStreamOffset kOffset8 = UINT64_C(0x3210BA9876543210);
const QuicIetfStreamOffset kOffset4 = UINT64_C(0x32109876);
const QuicIetfStreamOffset kOffset2 = UINT64_C(0x3456);
const QuicIetfStreamOffset kOffset1 = UINT64_C(0x3f);
const QuicIetfStreamOffset kOffset0 = UINT64_C(0x00);

// Structures used to create various ack frames.

// Defines an ack frame to feed through the framer/deframer.
struct ack_frame {
  uint64_t delay_time;
  const std::vector<QuicAckBlock>& ranges;
};

class QuicIetfFramerTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicIetfFramerTest()
      : start_(QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(0x10)),
        framer_(AllSupportedVersions(), start_, Perspective::IS_SERVER) {}

  // Utility functions to do actual framing/deframing.
  bool TryStreamFrame(char* packet_buffer,
                      size_t packet_buffer_size,
                      const char* xmit_packet_data,
                      size_t xmit_packet_data_size,
                      QuicIetfStreamId stream_id,
                      QuicIetfStreamOffset offset,
                      bool fin_bit,
                      bool last_frame_bit,
                      QuicIetfFrameType frame_type) {
    // initialize a writer so that the serialized packet is placed in
    // packet_buffer.
    QuicDataWriter writer(packet_buffer_size, packet_buffer,
                          NETWORK_BYTE_ORDER);  // do not really care
                                                // about endianness.

    // set up to define the source frame we wish to send.
    QuicStreamFrame source_stream_frame(
        stream_id, fin_bit, offset, xmit_packet_data, xmit_packet_data_size);

    // write the frame to the packet buffer.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfStreamFrame(
        &framer_, source_stream_frame, last_frame_bit, &writer));
    // better have something in the packet buffer.
    EXPECT_NE(0u, writer.length());
    // now set up a reader to read in the thing in.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

    // read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, frame_type);

    // a StreamFrame to hold the results... we know the frame type,
    // put it into the QuicIetfStreamFrame
    QuicStreamFrame sink_stream_frame;

    EXPECT_TRUE(QuicFramerPeer::ProcessIetfStreamFrame(
        &framer_, &reader, received_frame_type, &sink_stream_frame));

    // Now check that the streamid, fin-bit, offset, and
    // data len all match the input.
    EXPECT_EQ(received_frame_type, frame_type);
    EXPECT_EQ(sink_stream_frame.stream_id, source_stream_frame.stream_id);
    EXPECT_EQ(sink_stream_frame.fin, source_stream_frame.fin);
    EXPECT_EQ(sink_stream_frame.data_length, source_stream_frame.data_length);
    if (frame_type & IETF_STREAM_FRAME_OFF_BIT) {
      // there was an offset in the frame, see if xmit and rcv vales equal.
      EXPECT_EQ(sink_stream_frame.offset, source_stream_frame.offset);
    } else {
      // offset not in frame, so it better come out 0.
      EXPECT_EQ(sink_stream_frame.offset, 0u);
    }
    EXPECT_NE(sink_stream_frame.data_buffer, nullptr);
    EXPECT_NE(source_stream_frame.data_buffer, nullptr);
    EXPECT_EQ(
        strcmp(sink_stream_frame.data_buffer, source_stream_frame.data_buffer),
        0);
    return true;
  }

  // Overall ack frame encode/decode/compare function
  //  Encodes an ack frame as specified at |*frame|
  //  Then decodes the frame,
  //  Then compares the two
  // Does some basic checking:
  //   - did the writer write something?
  //   - did the reader read the entire packet?
  //   - did the things the reader read match what the writer wrote?
  // Returns true if it all worked false if not.
  bool TryAckFrame(char* packet_buffer,
                   size_t packet_buffer_size,
                   struct ack_frame* frame) {
    // Make a writer so that the serialized packet is placed in
    // packet_buffer.
    QuicDataWriter writer(packet_buffer_size, packet_buffer,
                          NETWORK_BYTE_ORDER);

    QuicAckFrame transmit_frame = InitAckFrame(frame->ranges);
    transmit_frame.ack_delay_time =
        QuicTime::Delta::FromMicroseconds(frame->delay_time);
    QUIC_LOG(INFO) << "XXXXXXXXXX transmit frame is " << transmit_frame;

    // write the frame to the packet buffer.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfAckFrameAndTypeByte(
        &framer_, transmit_frame, &writer));
    // better have something in the packet buffer.
    EXPECT_NE(0u, writer.length());

    // now set up a reader to read in the thing in.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

    // read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_ACK);

    // an AckFrame to hold the results
    QuicAckFrame receive_frame;

    EXPECT_TRUE(QuicFramerPeer::ProcessIetfAckFrame(
        &framer_, &reader, received_frame_type, &receive_frame));

    // Now check that things are correct
    EXPECT_EQ(transmit_frame.largest_acked, receive_frame.largest_acked);
    // The ~0x7 needs some explaining.  The ack frame format down shifts the
    // delay time by 3 (divide by 8) to allow for greater ranges in delay time.
    // Therefore, if we give the framer a delay time that is not an
    // even multiple of 8, the value that the deframer produces will
    // not be the same as what the framer got. The downshift on
    // framing and upshift on deframing results in clearing the 3
    // low-order bits ... The masking basically does the same thing,
    // so the compare works properly.
    EXPECT_EQ(transmit_frame.ack_delay_time.ToMicroseconds() & ~0x7,
              receive_frame.ack_delay_time.ToMicroseconds());
    EXPECT_EQ(transmit_frame.packets.NumIntervals(),
              receive_frame.packets.NumIntervals());
    // now go through the two sets of intervals....
    auto xmit_itr = transmit_frame.packets.begin();  // first range
    auto recv_itr = receive_frame.packets.begin();   // first range
    while (xmit_itr != transmit_frame.packets.end()) {
      EXPECT_EQ(xmit_itr->max(), recv_itr->max());
      EXPECT_EQ(xmit_itr->min(), recv_itr->min());
      xmit_itr++;
      recv_itr++;
    }
    return true;
  }

  QuicTime start_;
  QuicFramer framer_;
};

struct stream_frame_variant {
  QuicIetfStreamId stream_id;
  QuicIetfStreamOffset offset;
  bool fin_bit;
  bool last_frame_bit;
  uint8_t frame_type;
} stream_frame_to_test[] = {
#define IETF_STREAM0 (((uint8_t)IETF_STREAM))

#define IETF_STREAM1 (((uint8_t)IETF_STREAM) | IETF_STREAM_FRAME_FIN_BIT)

#define IETF_STREAM2 (((uint8_t)IETF_STREAM) | IETF_STREAM_FRAME_LEN_BIT)

#define IETF_STREAM3                                    \
  (((uint8_t)IETF_STREAM) | IETF_STREAM_FRAME_LEN_BIT | \
   IETF_STREAM_FRAME_FIN_BIT)

#define IETF_STREAM4 (((uint8_t)IETF_STREAM) | IETF_STREAM_FRAME_OFF_BIT)

#define IETF_STREAM5                                    \
  (((uint8_t)IETF_STREAM) | IETF_STREAM_FRAME_OFF_BIT | \
   IETF_STREAM_FRAME_FIN_BIT)

#define IETF_STREAM6                                    \
  (((uint8_t)IETF_STREAM) | IETF_STREAM_FRAME_OFF_BIT | \
   IETF_STREAM_FRAME_LEN_BIT)

#define IETF_STREAM7                                    \
  (((uint8_t)IETF_STREAM) | IETF_STREAM_FRAME_OFF_BIT | \
   IETF_STREAM_FRAME_LEN_BIT | IETF_STREAM_FRAME_FIN_BIT)

    {kStreamId8, kOffset8, true, false, IETF_STREAM7},
    {kStreamId8, kOffset8, false, false, IETF_STREAM6},
    {kStreamId8, kOffset4, true, false, IETF_STREAM7},
    {kStreamId8, kOffset4, false, false, IETF_STREAM6},
    {kStreamId8, kOffset2, true, false, IETF_STREAM7},
    {kStreamId8, kOffset2, false, false, IETF_STREAM6},
    {kStreamId8, kOffset1, true, false, IETF_STREAM7},
    {kStreamId8, kOffset1, false, false, IETF_STREAM6},
    {kStreamId8, kOffset0, true, false, IETF_STREAM3},
    {kStreamId8, kOffset0, false, false, IETF_STREAM2},
    {kStreamId4, kOffset8, true, false, IETF_STREAM7},
    {kStreamId4, kOffset8, false, false, IETF_STREAM6},
    {kStreamId4, kOffset4, true, false, IETF_STREAM7},
    {kStreamId4, kOffset4, false, false, IETF_STREAM6},
    {kStreamId4, kOffset2, true, false, IETF_STREAM7},
    {kStreamId4, kOffset2, false, false, IETF_STREAM6},
    {kStreamId4, kOffset1, true, false, IETF_STREAM7},
    {kStreamId4, kOffset1, false, false, IETF_STREAM6},
    {kStreamId4, kOffset0, true, false, IETF_STREAM3},
    {kStreamId4, kOffset0, false, false, IETF_STREAM2},
    {kStreamId2, kOffset8, true, false, IETF_STREAM7},
    {kStreamId2, kOffset8, false, false, IETF_STREAM6},
    {kStreamId2, kOffset4, true, false, IETF_STREAM7},
    {kStreamId2, kOffset4, false, false, IETF_STREAM6},
    {kStreamId2, kOffset2, true, false, IETF_STREAM7},
    {kStreamId2, kOffset2, false, false, IETF_STREAM6},
    {kStreamId2, kOffset1, true, false, IETF_STREAM7},
    {kStreamId2, kOffset1, false, false, IETF_STREAM6},
    {kStreamId2, kOffset0, true, false, IETF_STREAM3},
    {kStreamId2, kOffset0, false, false, IETF_STREAM2},
    {kStreamId1, kOffset8, true, false, IETF_STREAM7},
    {kStreamId1, kOffset8, false, false, IETF_STREAM6},
    {kStreamId1, kOffset4, true, false, IETF_STREAM7},
    {kStreamId1, kOffset4, false, false, IETF_STREAM6},
    {kStreamId1, kOffset2, true, false, IETF_STREAM7},
    {kStreamId1, kOffset2, false, false, IETF_STREAM6},
    {kStreamId1, kOffset1, true, false, IETF_STREAM7},
    {kStreamId1, kOffset1, false, false, IETF_STREAM6},
    {kStreamId1, kOffset0, true, false, IETF_STREAM3},
    {kStreamId1, kOffset0, false, false, IETF_STREAM2},
    {kStreamId0, kOffset8, true, false, IETF_STREAM7},
    {kStreamId0, kOffset8, false, false, IETF_STREAM6},
    {kStreamId0, kOffset4, true, false, IETF_STREAM7},
    {kStreamId0, kOffset4, false, false, IETF_STREAM6},
    {kStreamId0, kOffset2, true, false, IETF_STREAM7},
    {kStreamId0, kOffset2, false, false, IETF_STREAM6},
    {kStreamId0, kOffset1, true, false, IETF_STREAM7},
    {kStreamId0, kOffset1, false, false, IETF_STREAM6},
    {kStreamId0, kOffset0, true, false, IETF_STREAM3},
    {kStreamId0, kOffset0, false, false, IETF_STREAM2},

    {kStreamId8, kOffset8, true, true, IETF_STREAM7},
    {kStreamId8, kOffset8, false, true, IETF_STREAM6},
    {kStreamId8, kOffset4, true, true, IETF_STREAM7},
    {kStreamId8, kOffset4, false, true, IETF_STREAM6},
    {kStreamId8, kOffset2, true, true, IETF_STREAM7},
    {kStreamId8, kOffset2, false, true, IETF_STREAM6},
    {kStreamId8, kOffset1, true, true, IETF_STREAM7},
    {kStreamId8, kOffset1, false, true, IETF_STREAM6},
    {kStreamId8, kOffset0, true, true, IETF_STREAM3},
    {kStreamId8, kOffset0, false, true, IETF_STREAM2},
    {kStreamId4, kOffset8, true, true, IETF_STREAM7},
    {kStreamId4, kOffset8, false, true, IETF_STREAM6},
    {kStreamId4, kOffset4, true, true, IETF_STREAM7},
    {kStreamId4, kOffset4, false, true, IETF_STREAM6},
    {kStreamId4, kOffset2, true, true, IETF_STREAM7},
    {kStreamId4, kOffset2, false, true, IETF_STREAM6},
    {kStreamId4, kOffset1, true, true, IETF_STREAM7},
    {kStreamId4, kOffset1, false, true, IETF_STREAM6},
    {kStreamId4, kOffset0, true, true, IETF_STREAM3},
    {kStreamId4, kOffset0, false, true, IETF_STREAM2},
    {kStreamId2, kOffset8, true, true, IETF_STREAM7},
    {kStreamId2, kOffset8, false, true, IETF_STREAM6},
    {kStreamId2, kOffset4, true, true, IETF_STREAM7},
    {kStreamId2, kOffset4, false, true, IETF_STREAM6},
    {kStreamId2, kOffset2, true, true, IETF_STREAM7},
    {kStreamId2, kOffset2, false, true, IETF_STREAM6},
    {kStreamId2, kOffset1, true, true, IETF_STREAM7},
    {kStreamId2, kOffset1, false, true, IETF_STREAM6},
    {kStreamId2, kOffset0, true, true, IETF_STREAM3},
    {kStreamId2, kOffset0, false, true, IETF_STREAM2},
    {kStreamId1, kOffset8, true, true, IETF_STREAM7},
    {kStreamId1, kOffset8, false, true, IETF_STREAM6},
    {kStreamId1, kOffset4, true, true, IETF_STREAM7},
    {kStreamId1, kOffset4, false, true, IETF_STREAM6},
    {kStreamId1, kOffset2, true, true, IETF_STREAM7},
    {kStreamId1, kOffset2, false, true, IETF_STREAM6},
    {kStreamId1, kOffset1, true, true, IETF_STREAM7},
    {kStreamId1, kOffset1, false, true, IETF_STREAM6},
    {kStreamId1, kOffset0, true, true, IETF_STREAM3},
    {kStreamId1, kOffset0, false, true, IETF_STREAM2},
    {kStreamId0, kOffset8, true, true, IETF_STREAM7},
    {kStreamId0, kOffset8, false, true, IETF_STREAM6},
    {kStreamId0, kOffset4, true, true, IETF_STREAM7},
    {kStreamId0, kOffset4, false, true, IETF_STREAM6},
    {kStreamId0, kOffset2, true, true, IETF_STREAM7},
    {kStreamId0, kOffset2, false, true, IETF_STREAM6},
    {kStreamId0, kOffset1, true, true, IETF_STREAM7},
    {kStreamId0, kOffset1, false, true, IETF_STREAM6},
    {kStreamId0, kOffset0, true, true, IETF_STREAM3},
    {kStreamId0, kOffset0, false, true, IETF_STREAM2},

    // try some cases where the offset is _not_ present; we will give
    // the framer a non-0 offset; however, if we say that there is to be
    // no offset, the de-framer should come up with 0...
    {kStreamId8, kOffset8, true, true, IETF_STREAM3},
    {kStreamId8, kOffset8, false, true, IETF_STREAM2},
    {kStreamId8, kOffset8, true, false, IETF_STREAM3},
    {kStreamId8, kOffset8, false, false, IETF_STREAM2},

    {0, 0, false, false, IETF_STREAM6},
};

TEST_F(QuicIetfFramerTest, StreamFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  const char* transmit_packet_data =
      "this is a test of some packet data, "
      "can do a simple strcmp to see if the "
      "input and output are the same!";

  size_t transmit_packet_data_len = strlen(transmit_packet_data) + 1;
  struct stream_frame_variant* variant = stream_frame_to_test;
  while (variant->stream_id != 0) {
    EXPECT_TRUE(TryStreamFrame(
        packet_buffer, sizeof(packet_buffer), transmit_packet_data,
        transmit_packet_data_len, variant->stream_id, variant->offset,
        variant->fin_bit, variant->last_frame_bit,
        static_cast<QuicIetfFrameType>(variant->frame_type)));
    variant++;
  }
}

// tests for the ietf connection/application close frames.
// These are not regular enough to be table driven, so doing
// explicit tests is what we need to do...

TEST_F(QuicIetfFramerTest, ConnectionClose1) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  string test_string = "This is a test of the emergency broadcast system";
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfConnectionCloseFrame(
      &framer_, QuicIetfTransportErrorCodes::VERSION_NEGOTIATION_ERROR,
      test_string, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the thing in.
  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

  // read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, QuicIetfFrameType::IETF_CONNECTION_CLOSE);

  // a QuicConnectionCloseFrame to hold the results.
  QuicConnectionCloseFrame sink_frame;

  EXPECT_TRUE(QuicFramerPeer::ProcessIetfConnectionCloseFrame(
      &framer_, &reader, received_frame_type, &sink_frame));

  // Now check that received == sent
  EXPECT_EQ(sink_frame.error_code,
            static_cast<QuicErrorCode>(
                QuicIetfTransportErrorCodes::VERSION_NEGOTIATION_ERROR));
  EXPECT_EQ(sink_frame.error_details, test_string);
}

// test case of having no string. also the 0 error code,
TEST_F(QuicIetfFramerTest, ConnectionClose2) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  // empty string,
  string test_string;
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfConnectionCloseFrame(
      &framer_,
      QuicIetfTransportErrorCodes::NO_IETF_QUIC_ERROR,  // NO_ERROR == 0
      test_string, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the thing in.
  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

  // read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, QuicIetfFrameType::IETF_CONNECTION_CLOSE);

  // a QuicConnectionCloseFrame to hold the results.
  QuicConnectionCloseFrame sink_frame;

  EXPECT_TRUE(QuicFramerPeer::ProcessIetfConnectionCloseFrame(
      &framer_, &reader, received_frame_type, &sink_frame));

  // Now check that received == sent
  EXPECT_EQ(sink_frame.error_code,
            static_cast<QuicErrorCode>(
                QuicIetfTransportErrorCodes::NO_IETF_QUIC_ERROR));
  EXPECT_EQ(sink_frame.error_details, test_string);
}
// Set fields of the frame via a QuicConnectionClose object
// test case of having no string. also the 0 error code,
TEST_F(QuicIetfFramerTest, ConnectionClose3) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  // empty string,
  string test_string = "Ich Bin Ein Jelly Donut?";
  QuicConnectionCloseFrame sent_frame;
  sent_frame.error_code = static_cast<QuicErrorCode>(0);
  sent_frame.error_details = test_string;
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfConnectionCloseFrame(
      &framer_, sent_frame, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the thing in.
  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

  // read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, QuicIetfFrameType::IETF_CONNECTION_CLOSE);

  // a QuicConnectionCloseFrame to hold the results.
  QuicConnectionCloseFrame sink_frame;

  EXPECT_TRUE(QuicFramerPeer::ProcessIetfConnectionCloseFrame(
      &framer_, &reader, received_frame_type, &sink_frame));

  // Now check that received == sent
  EXPECT_EQ(sink_frame.error_code, static_cast<QuicErrorCode>(0));
  EXPECT_EQ(sink_frame.error_details, test_string);
}

TEST_F(QuicIetfFramerTest, ApplicationClose1) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  string test_string = "This is a test of the emergency broadcast system";
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfApplicationCloseFrame(
      &framer_,
      static_cast<uint16_t>(QuicErrorCode::QUIC_CRYPTO_VERSION_NOT_SUPPORTED),
      test_string, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the thing in.
  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

  // read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, QuicIetfFrameType::IETF_APPLICATION_CLOSE);

  // a QuicConnectionCloseFrame to hold the results.
  QuicConnectionCloseFrame sink_frame;

  EXPECT_TRUE(QuicFramerPeer::ProcessIetfApplicationCloseFrame(
      &framer_, &reader, received_frame_type, &sink_frame));

  // Now check that received == sent
  EXPECT_EQ(sink_frame.error_code,
            QuicErrorCode::QUIC_CRYPTO_VERSION_NOT_SUPPORTED);
  EXPECT_EQ(sink_frame.error_details, test_string);
}

// test case of having no string. also the 0 error code,
TEST_F(QuicIetfFramerTest, ApplicationClose2) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  // empty string,
  string test_string;
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfApplicationCloseFrame(
      &framer_, 0, test_string, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the thing in.
  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

  // read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, QuicIetfFrameType::IETF_APPLICATION_CLOSE);

  // a QuicConnectionCloseFrame to hold the results.
  QuicConnectionCloseFrame sink_frame;

  EXPECT_TRUE(QuicFramerPeer::ProcessIetfApplicationCloseFrame(
      &framer_, &reader, received_frame_type, &sink_frame));

  // Now check that received == sent
  EXPECT_EQ(sink_frame.error_code, 0);
  EXPECT_EQ(sink_frame.error_details, test_string);
}

// Set fields of the frame via a QuicConnectionClose object   wahoo
// test case of having no string. also the 0 error code,
TEST_F(QuicIetfFramerTest, ApplicationClose3) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  // empty string,
  string test_string = "Ich Bin Ein Jelly Donut?";
  QuicConnectionCloseFrame sent_frame;
  sent_frame.error_code = static_cast<QuicErrorCode>(0);
  sent_frame.error_details = test_string;
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfApplicationCloseFrame(
      &framer_, sent_frame, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the thing in.
  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

  // read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, QuicIetfFrameType::IETF_APPLICATION_CLOSE);

  // a QuicConnectionCloseFrame to hold the results.
  QuicConnectionCloseFrame sink_frame;

  EXPECT_TRUE(QuicFramerPeer::ProcessIetfApplicationCloseFrame(
      &framer_, &reader, received_frame_type, &sink_frame));

  // Now check that received == sent
  EXPECT_EQ(sink_frame.error_code, static_cast<QuicErrorCode>(0));
  EXPECT_EQ(sink_frame.error_details, test_string);
}

// Testing for the IETF ACK framer.
// clang-format off
struct ack_frame ack_frame_variants[] = {
  { 90000, {{1000, 2001}} },
  { 0, {{1000, 2001}} },
  { 1, {{1, 2}, {5, 6}} },
  { 63, {{1, 2}, {5, 6}} },
  { 64, {{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12}}},
  { 10000, {{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12}}},
  { 100000000, {{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12}}},
};
// clang-format on

TEST_F(QuicIetfFramerTest, AckFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  for (auto ack_frame_variant : ack_frame_variants) {
    QUIC_LOG(INFO) << "Doing an ack, delay = " << ack_frame_variant.delay_time;
    EXPECT_TRUE(
        TryAckFrame(packet_buffer, sizeof(packet_buffer), &ack_frame_variant));
  }
}

}  // namespace
}  // namespace test
}  // namespace net
