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
#include "net/quic/platform/api/quic_string.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_data_producer.h"


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

    // Write the frame to the packet buffer.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfStreamFrame(
        &framer_, source_stream_frame, last_frame_bit, &writer));
    // Better have something in the packet buffer.
    EXPECT_NE(0u, writer.length());
    // Now set up a reader to read in the frame.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

    // Read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, frame_type);

    // A StreamFrame to hold the results... we know the frame type,
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
      // There was an offset in the frame, see if xmit and rcv vales equal.
      EXPECT_EQ(sink_stream_frame.offset, source_stream_frame.offset);
    } else {
      // Offset not in frame, so it better come out 0.
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

    // Write the frame to the packet buffer.
    EXPECT_TRUE(
        QuicFramerPeer::AppendIetfAckFrame(&framer_, transmit_frame, &writer));
    // Better have something in the packet buffer.
    EXPECT_NE(0u, writer.length());

    // Now set up a reader to read in the frame.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

    // read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_ACK);

    // an AckFrame to hold the results
    QuicAckFrame receive_frame;

    EXPECT_TRUE(QuicFramerPeer::ProcessIetfAckFrame(
        &framer_, &reader, received_frame_type, &receive_frame));

    // Now check that the received frame matches the sent frame.
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

  // encode, decode, and check a Path Challenge frame.
  bool TryPathChallengeFrame(char* packet_buffer,
                             size_t packet_buffer_size,
                             const QuicPathFrameBuffer& data) {
    // Make a writer so that the serialized packet is placed in
    // packet_buffer.
    QuicDataWriter writer(packet_buffer_size, packet_buffer,
                          NETWORK_BYTE_ORDER);

    QuicPathChallengeFrame transmit_frame(0, data);

    // write the frame to the packet buffer.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfPathChallengeFrame(
        &framer_, transmit_frame, &writer));

    // Check for correct length in the packet buffer.
    EXPECT_EQ(kQuicPathChallengeFrameSize, writer.length());

    // now set up a reader to read in the frame.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

    // read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_PATH_CHALLENGE);

    QuicPathChallengeFrame receive_frame;

    EXPECT_TRUE(QuicFramerPeer::ProcessIetfPathChallengeFrame(&framer_, &reader,
                                                              &receive_frame));

    // Now check that the received frame matches the sent frame.
    EXPECT_EQ(
        0, memcmp(transmit_frame.data_buffer.data(),
                  receive_frame.data_buffer.data(), kQuicPathFrameBufferSize));
    return true;
  }

  // encode, decode, and check a Path Response frame.
  bool TryPathResponseFrame(char* packet_buffer,
                            size_t packet_buffer_size,
                            const QuicPathFrameBuffer& data) {
    // Make a writer so that the serialized packet is placed in
    // packet_buffer.
    QuicDataWriter writer(packet_buffer_size, packet_buffer,
                          NETWORK_BYTE_ORDER);

    QuicPathResponseFrame transmit_frame(0, data);

    // Write the frame to the packet buffer.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfPathResponseFrame(
        &framer_, transmit_frame, &writer));

    // Check for correct length in the packet buffer.
    EXPECT_EQ(kQuicPathResponseFrameSize, writer.length());

    // Set up a reader to read in the frame.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

    // Read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_PATH_RESPONSE);

    QuicPathResponseFrame receive_frame;

    EXPECT_TRUE(QuicFramerPeer::ProcessIetfPathResponseFrame(&framer_, &reader,
                                                             &receive_frame));

    // Now check that the received frame matches the sent frame.
    EXPECT_EQ(
        0, memcmp(transmit_frame.data_buffer.data(),
                  receive_frame.data_buffer.data(), kQuicPathFrameBufferSize));
    return true;
  }

  // Test the Serialization/deserialization of a Reset Stream Frame.
  void TryResetFrame(char* packet_buffer,
                     size_t packet_buffer_size,
                     QuicStreamId stream_id,
                     QuicRstStreamErrorCode error_code,
                     QuicStreamOffset final_offset) {
    // Initialize a writer so that the serialized packet is placed in
    // packet_buffer.
    QuicDataWriter writer(packet_buffer_size, packet_buffer,
                          NETWORK_BYTE_ORDER);

    QuicRstStreamFrame transmit_frame(static_cast<QuicControlFrameId>(1),
                                      stream_id, error_code, final_offset);

    // Write the frame to the packet buffer.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfResetStreamFrame(
        &framer_, transmit_frame, &writer));
    // Check that the size of the serialzed frame is in the allowed range.
    EXPECT_LT(4u, writer.length());
    EXPECT_GT(20u, writer.length());
    // Now set up a reader to read in the thing in.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

    // Read in the frame type and check that it is IETF_RST_STREAM.
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, QuicIetfFrameType::IETF_RST_STREAM);

    // A QuicRstStreamFrame to hold the results
    QuicRstStreamFrame receive_frame;
    EXPECT_TRUE(QuicFramerPeer::ProcessIetfResetStreamFrame(&framer_, &reader,
                                                            &receive_frame));

    // Now check that the received values match the input.
    EXPECT_EQ(receive_frame.stream_id, transmit_frame.stream_id);
    EXPECT_EQ(receive_frame.error_code, transmit_frame.error_code);
    EXPECT_EQ(receive_frame.byte_offset, transmit_frame.byte_offset);
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

TEST_F(QuicIetfFramerTest, ConnectionCloseEmptyString) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  // empty string,
  QuicString test_string = "Ich Bin Ein Jelly Donut?";
  QuicConnectionCloseFrame sent_frame;
  sent_frame.error_code = static_cast<QuicErrorCode>(0);
  sent_frame.error_details = test_string;
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfConnectionCloseFrame(
      &framer_, sent_frame, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the frame.
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

TEST_F(QuicIetfFramerTest, ApplicationCloseEmptyString) {
  char packet_buffer[kNormalPacketBufferSize];

  // initialize a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  // empty string,
  QuicString test_string = "Ich Bin Ein Jelly Donut?";
  QuicConnectionCloseFrame sent_frame;
  sent_frame.error_code = static_cast<QuicErrorCode>(0);
  sent_frame.error_details = test_string;
  // write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfApplicationCloseFrame(
      &framer_, sent_frame, &writer));

  // better have something in the packet buffer.
  EXPECT_NE(0u, writer.length());

  // now set up a reader to read in the frame.
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
    EXPECT_TRUE(
        TryAckFrame(packet_buffer, sizeof(packet_buffer), &ack_frame_variant));
  }
}

TEST_F(QuicIetfFramerTest, PaddingEntirePacket) {
  char packet_buffer[kNormalPacketBufferSize];

  // ensure that buffer is not all 0 prior to the test.
  memset(packet_buffer, 0xff, sizeof(packet_buffer));

  // Set up the writer and transmit QuicPaddingFrame
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);
  QuicPaddingFrame transmit_frame(sizeof(packet_buffer));

  // Fill buffer with padding.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfPaddingFrame(&framer_, transmit_frame,
                                                     &writer));

  // better have written to the entire packet buffer.
  EXPECT_EQ(kNormalPacketBufferSize, writer.length());

  // see if entire buffer is 0
  for (auto i = 0; i != sizeof(packet_buffer); i++) {
    EXPECT_EQ(0, packet_buffer[i])
        << "Packet_buffer[" << i << "] is " << packet_buffer[i] << " not 0x00";
  }

  // set up reader and empty receive QuicPaddingFrame.
  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
  QuicPaddingFrame receive_frame;

  // read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, 0u);

  // deframe it
  QuicFramerPeer::ProcessIetfPaddingFrame(&framer_, &reader, &receive_frame);

  // Now check that received == sent
  EXPECT_EQ(transmit_frame.num_padding_bytes, receive_frame.num_padding_bytes);
  int packet_buffer_size = static_cast<int>(sizeof(packet_buffer));
  EXPECT_EQ(packet_buffer_size, receive_frame.num_padding_bytes);
  EXPECT_EQ(packet_buffer_size, transmit_frame.num_padding_bytes);
}

// Place a padding frame between two non-padding frames:
//    app_close
//    padding
//    connection_close
// we do a loop, with different amounts of padding in each.
TEST_F(QuicIetfFramerTest, PaddingSandwich) {
  int pad_lengths[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 0};
  int* pad_length = pad_lengths;
  while (*pad_length) {
    char packet_buffer[kNormalPacketBufferSize];

    // ensure that buffer is not all 0 prior to the test.
    memset(packet_buffer, 0xff, sizeof(packet_buffer));

    // Set up the writer and transmit Quic...Frames
    QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                          NETWORK_BYTE_ORDER);
    QuicPaddingFrame transmit_pad_frame(*pad_length);
    QuicString app_close_test_string = "Ich Bin Ein Jelly Donut?";
    QuicConnectionCloseFrame transmit_app_close_frame;
    transmit_app_close_frame.error_code = static_cast<QuicErrorCode>(0);
    transmit_app_close_frame.error_details = app_close_test_string;

    QuicString conn_close_test_string = "I am a Berliner?";
    QuicConnectionCloseFrame transmit_conn_close_frame;
    transmit_conn_close_frame.error_code = static_cast<QuicErrorCode>(0);
    transmit_conn_close_frame.error_details = conn_close_test_string;

    // Put in the frames. App close first.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfApplicationCloseFrame(
        &framer_, transmit_app_close_frame, &writer));

    size_t pre_padding_len = writer.length();
    // padding next.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfPaddingFrame(
        &framer_, transmit_pad_frame, &writer));
    size_t post_padding_len = writer.length();
    EXPECT_EQ(static_cast<int>(post_padding_len - pre_padding_len),
              *pad_length);

    // finally, connection close
    EXPECT_TRUE(QuicFramerPeer::AppendIetfConnectionCloseFrame(
        &framer_, transmit_conn_close_frame, &writer));

    // see if buffer from offset pre_padding_len, for *pad_len, is 0
    for (auto i = pre_padding_len; i != pre_padding_len + (*pad_length); i++) {
      EXPECT_EQ(0, packet_buffer[i]) << "Packet_buffer[" << i << "] is "
                                     << packet_buffer[i] << " not 0x00";
    }

    // set up reader and empty receive QuicFrames.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
    QuicPaddingFrame receive_pad_frame;
    QuicConnectionCloseFrame receive_app_close_frame;
    QuicConnectionCloseFrame receive_conn_close_frame;

    // read in the frame type and data for the app-close frame
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_APPLICATION_CLOSE);
    EXPECT_TRUE(QuicFramerPeer::ProcessIetfApplicationCloseFrame(
        &framer_, &reader, received_frame_type, &receive_app_close_frame));

    // Now the padding.
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_PADDING);
    QuicFramerPeer::ProcessIetfPaddingFrame(&framer_, &reader,
                                            &receive_pad_frame);
    // check that pad size is correct
    EXPECT_EQ(receive_pad_frame.num_padding_bytes, *pad_length);

    // Now get the connection close frame.
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_CONNECTION_CLOSE);
    EXPECT_TRUE(QuicFramerPeer::ProcessIetfConnectionCloseFrame(
        &framer_, &reader, received_frame_type, &receive_conn_close_frame));

    pad_length++;
  }
}

TEST_F(QuicIetfFramerTest, PathChallengeFrame) {
  // Double-braces needed on some platforms due to
  // https://bugs.llvm.org/show_bug.cgi?id=21629
  QuicPathFrameBuffer buffer0 = {{0, 0, 0, 0, 0, 0, 0, 0}};
  QuicPathFrameBuffer buffer1 = {
      {0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe5, 0xf7}};
  char packet_buffer[kNormalPacketBufferSize];
  EXPECT_TRUE(
      TryPathChallengeFrame(packet_buffer, sizeof(packet_buffer), buffer0));
  EXPECT_TRUE(
      TryPathChallengeFrame(packet_buffer, sizeof(packet_buffer), buffer1));
}

TEST_F(QuicIetfFramerTest, PathResponseFrame) {
  // Double-braces needed on some platforms due to
  // https://bugs.llvm.org/show_bug.cgi?id=21629
  QuicPathFrameBuffer buffer0 = {{0, 0, 0, 0, 0, 0, 0, 0}};
  QuicPathFrameBuffer buffer1 = {
      {0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe5, 0xf7}};
  char packet_buffer[kNormalPacketBufferSize];
  EXPECT_TRUE(
      TryPathResponseFrame(packet_buffer, sizeof(packet_buffer), buffer0));
  EXPECT_TRUE(
      TryPathResponseFrame(packet_buffer, sizeof(packet_buffer), buffer1));
}

TEST_F(QuicIetfFramerTest, ResetStreamFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  struct resets {
    QuicStreamId stream_id;
    QuicRstStreamErrorCode error_code;
    QuicStreamOffset final_offset;
  } reset_frames[] = {
      {0, QUIC_STREAM_NO_ERROR, 0}, {0x10, QUIC_HEADERS_TOO_LARGE, 0x300},
  };
  for (auto reset : reset_frames) {
    TryResetFrame(packet_buffer, sizeof(packet_buffer), reset.stream_id,
                  reset.error_code, reset.final_offset);
  }
}

TEST_F(QuicIetfFramerTest, StopSendingFrame) {
  char packet_buffer[kNormalPacketBufferSize];

  // Make a writer so that the serialized packet is placed in
  // packet_buffer.
  QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                        NETWORK_BYTE_ORDER);

  QuicStopSendingFrame transmit_frame;
  transmit_frame.stream_id = 12345;
  transmit_frame.application_error_code = 543;

  // Write the frame to the packet buffer.
  EXPECT_TRUE(QuicFramerPeer::AppendIetfStopSendingFrame(
      &framer_, transmit_frame, &writer));
  // Check that the number of bytes in the buffer is in the
  // allowed range.
  EXPECT_LE(4u, writer.length());
  EXPECT_GE(11u, writer.length());

  QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);

  // Read in the frame type
  uint8_t received_frame_type;
  EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
  EXPECT_EQ(received_frame_type, IETF_STOP_SENDING);

  // A frame to hold the results
  QuicStopSendingFrame receive_frame;

  EXPECT_TRUE(QuicFramerPeer::ProcessIetfStopSendingFrame(&framer_, &reader,
                                                          &receive_frame));

  // Verify that the transmitted and received values are the same.
  EXPECT_EQ(receive_frame.stream_id, 12345u);
  EXPECT_EQ(receive_frame.application_error_code, 543u);
  EXPECT_EQ(receive_frame.stream_id, transmit_frame.stream_id);
  EXPECT_EQ(receive_frame.application_error_code,
            transmit_frame.application_error_code);
}

TEST_F(QuicIetfFramerTest, MaxDataFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  QuicStreamOffset window_sizes[] = {0,       1,        2,        5,       10,
                                     20,      50,       100,      200,     500,
                                     1000000, kOffset8, kOffset4, kOffset2};
  for (QuicStreamOffset window_size : window_sizes) {
    memset(packet_buffer, 0, sizeof(packet_buffer));

    // Set up the writer and transmit QuicWindowUpdateFrame
    QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                          NETWORK_BYTE_ORDER);
    QuicWindowUpdateFrame transmit_frame(0, 99, window_size);

    // Add the frame.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfMaxDataFrame(&framer_, transmit_frame,
                                                       &writer));

    // Check that the number of bytes in the buffer is in the expected range.
    EXPECT_LE(2u, writer.length());
    EXPECT_GE(9u, writer.length());

    // Set up reader and an empty QuicWindowUpdateFrame
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
    QuicWindowUpdateFrame receive_frame;

    // Read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_MAX_DATA);

    // Deframe it
    EXPECT_TRUE(QuicFramerPeer::ProcessIetfMaxDataFrame(&framer_, &reader,
                                                        &receive_frame));

    // Now check that the received data equals the sent data.
    EXPECT_EQ(transmit_frame.byte_offset, window_size);
    EXPECT_EQ(transmit_frame.byte_offset, receive_frame.byte_offset);
    EXPECT_EQ(0u, receive_frame.stream_id);
  }
}

TEST_F(QuicIetfFramerTest, MaxStreamDataFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  QuicStreamOffset window_sizes[] = {0,       1,        2,        5,       10,
                                     20,      50,       100,      200,     500,
                                     1000000, kOffset8, kOffset4, kOffset2};
  QuicIetfStreamId stream_ids[] = {kStreamId4, kStreamId2, kStreamId1,
                                   kStreamId0};

  for (QuicIetfStreamId stream_id : stream_ids) {
    for (QuicStreamOffset window_size : window_sizes) {
      memset(packet_buffer, 0, sizeof(packet_buffer));

      // Set up the writer and transmit QuicWindowUpdateFrame
      QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                            NETWORK_BYTE_ORDER);
      QuicWindowUpdateFrame transmit_frame(0, stream_id, window_size);

      // Add the frame.
      EXPECT_TRUE(QuicFramerPeer::AppendIetfMaxStreamDataFrame(
          &framer_, transmit_frame, &writer));

      // Check that number of bytes in the buffer is in the expected range.
      EXPECT_LE(3u, writer.length());
      EXPECT_GE(17u, writer.length());

      // Set up reader and empty receive QuicPaddingFrame.
      QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
      QuicWindowUpdateFrame receive_frame;

      // Read in the frame type
      uint8_t received_frame_type;
      EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
      EXPECT_EQ(received_frame_type, IETF_MAX_STREAM_DATA);

      // Deframe it
      EXPECT_TRUE(QuicFramerPeer::ProcessIetfMaxStreamDataFrame(
          &framer_, &reader, &receive_frame));

      // Now check that received data and sent data are equal.
      EXPECT_EQ(transmit_frame.byte_offset, window_size);
      EXPECT_EQ(transmit_frame.byte_offset, receive_frame.byte_offset);
      EXPECT_EQ(stream_id, receive_frame.stream_id);
      EXPECT_EQ(transmit_frame.stream_id, receive_frame.stream_id);
    }
  }
}

TEST_F(QuicIetfFramerTest, MaxStreamIdFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  QuicIetfStreamId stream_ids[] = {kStreamId4, kStreamId2, kStreamId1,
                                   kStreamId0};

  for (QuicIetfStreamId stream_id : stream_ids) {
    memset(packet_buffer, 0, sizeof(packet_buffer));

    // Set up the writer and transmit QuicIetfMaxStreamIdFrame
    QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                          NETWORK_BYTE_ORDER);
    QuicIetfMaxStreamIdFrame transmit_frame(0, stream_id);

    // Add the frame.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfMaxStreamIdFrame(
        &framer_, transmit_frame, &writer));

    // Check that buffer length is in the expected range
    EXPECT_LE(2u, writer.length());
    EXPECT_GE(9u, writer.length());

    // Set up reader and empty receive QuicPaddingFrame.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
    QuicIetfMaxStreamIdFrame receive_frame;

    // Read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_MAX_STREAM_ID);

    // Deframe it
    EXPECT_TRUE(QuicFramerPeer::ProcessIetfMaxStreamIdFrame(&framer_, &reader,
                                                            &receive_frame));

    // Now check that received and sent data are equivalent
    EXPECT_EQ(stream_id, receive_frame.max_stream_id);
    EXPECT_EQ(transmit_frame.max_stream_id, receive_frame.max_stream_id);
  }
}

TEST_F(QuicIetfFramerTest, BlockedFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  QuicStreamOffset offsets[] = {kOffset8, kOffset4, kOffset2, kOffset1,
                                kOffset0};

  for (QuicStreamOffset offset : offsets) {
    memset(packet_buffer, 0, sizeof(packet_buffer));

    // Set up the writer and transmit QuicIetfBlockedFrame
    QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                          NETWORK_BYTE_ORDER);
    QuicIetfBlockedFrame transmit_frame(0, offset);

    // Add the frame.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfBlockedFrame(&framer_, transmit_frame,
                                                       &writer));

    // Check that buffer length is in the expected range
    EXPECT_LE(2u, writer.length());
    EXPECT_GE(9u, writer.length());

    // Set up reader and empty receive QuicFrame.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
    QuicIetfBlockedFrame receive_frame;

    // Read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_BLOCKED);

    // Deframe it
    EXPECT_TRUE(QuicFramerPeer::ProcessIetfBlockedFrame(&framer_, &reader,
                                                        &receive_frame));

    // Check that received and sent data are equivalent
    EXPECT_EQ(offset, receive_frame.offset);
    EXPECT_EQ(transmit_frame.offset, receive_frame.offset);
  }
}

TEST_F(QuicIetfFramerTest, StreamBlockedFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  QuicStreamOffset offsets[] = {0,       1,        2,        5,       10,
                                20,      50,       100,      200,     500,
                                1000000, kOffset8, kOffset4, kOffset2};
  QuicIetfStreamId stream_ids[] = {kStreamId4, kStreamId2, kStreamId1,
                                   kStreamId0};

  for (QuicIetfStreamId stream_id : stream_ids) {
    for (QuicStreamOffset offset : offsets) {
      memset(packet_buffer, 0, sizeof(packet_buffer));

      // Set up the writer and transmit QuicWindowUpdateFrame
      QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                            NETWORK_BYTE_ORDER);
      QuicWindowUpdateFrame transmit_frame(0, stream_id, offset);

      // Add the frame.
      EXPECT_TRUE(QuicFramerPeer::AppendIetfStreamBlockedFrame(
          &framer_, transmit_frame, &writer));

      // Check that number of bytes in the buffer is in the expected range.
      EXPECT_LE(3u, writer.length());
      EXPECT_GE(17u, writer.length());

      // Set up reader and empty receive QuicPaddingFrame.
      QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
      QuicWindowUpdateFrame receive_frame;

      // Read in the frame type
      uint8_t received_frame_type;
      EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
      EXPECT_EQ(received_frame_type, IETF_STREAM_BLOCKED);

      // Deframe it
      EXPECT_TRUE(QuicFramerPeer::ProcessIetfStreamBlockedFrame(
          &framer_, &reader, &receive_frame));

      // Now check that received == sent
      EXPECT_EQ(transmit_frame.byte_offset, offset);
      EXPECT_EQ(transmit_frame.byte_offset, receive_frame.byte_offset);
      EXPECT_EQ(stream_id, receive_frame.stream_id);
      EXPECT_EQ(transmit_frame.stream_id, receive_frame.stream_id);
    }
  }
}

TEST_F(QuicIetfFramerTest, StreamIdBlockedFrame) {
  char packet_buffer[kNormalPacketBufferSize];
  QuicIetfStreamId stream_ids[] = {kStreamId4, kStreamId2, kStreamId1,
                                   kStreamId0};

  for (QuicIetfStreamId stream_id : stream_ids) {
    memset(packet_buffer, 0, sizeof(packet_buffer));

    // Set up the writer and transmit QuicIetfStreamIdBlockedFrame
    QuicDataWriter writer(sizeof(packet_buffer), packet_buffer,
                          NETWORK_BYTE_ORDER);
    QuicIetfStreamIdBlockedFrame transmit_frame(0, stream_id);

    // Add the frame.
    EXPECT_TRUE(QuicFramerPeer::AppendIetfStreamIdBlockedFrame(
        &framer_, transmit_frame, &writer));

    // Check that buffer length is in the expected range
    EXPECT_LE(2u, writer.length());
    EXPECT_GE(9u, writer.length());

    // Set up reader and empty receive QuicPaddingFrame.
    QuicDataReader reader(packet_buffer, writer.length(), NETWORK_BYTE_ORDER);
    QuicIetfStreamIdBlockedFrame receive_frame;

    // Read in the frame type
    uint8_t received_frame_type;
    EXPECT_TRUE(reader.ReadUInt8(&received_frame_type));
    EXPECT_EQ(received_frame_type, IETF_STREAM_ID_BLOCKED);

    // Deframe it
    EXPECT_TRUE(QuicFramerPeer::ProcessIetfStreamIdBlockedFrame(
        &framer_, &reader, &receive_frame));

    // Now check that received == sent
    EXPECT_EQ(stream_id, receive_frame.stream_id);
    EXPECT_EQ(transmit_frame.stream_id, receive_frame.stream_id);
  }
}

}  // namespace
}  // namespace test
}  // namespace net
