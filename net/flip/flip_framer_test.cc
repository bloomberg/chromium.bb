// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "base/scoped_ptr.h"

#include "flip_framer.h"  // cross-google3 directory naming.
#include "flip_protocol.h"
#include "flip_frame_builder.h"
#include "testing/platform_test.h"

namespace flip {

class FlipFramerTest : public PlatformTest {
 public:
  virtual void TearDown() {}
};

class TestFlipVisitor : public FlipFramerVisitorInterface  {
 public:
  explicit TestFlipVisitor()
    : error_count_(0),
      syn_frame_count_(0),
      syn_reply_frame_count_(0),
      data_bytes_(0),
      fin_frame_count_(0),
      fin_flag_count_(0),
      zero_length_data_frame_count_(0) {
  }

  void OnError(FlipFramer* f) {
    error_count_++;
  }

  void OnStreamFrameData(FlipStreamId stream_id,
                         const char* data,
                         size_t len) {
    if (len == 0)
      zero_length_data_frame_count_++;

    data_bytes_ += len;
#ifdef TEST_LOGGING
    std::cerr << "OnStreamFrameData(" << stream_id << ", \"";
    if (len > 0) {
      for (size_t i = 0 ; i < len; ++i) {
        std::cerr << std::hex << (0xFF & (unsigned int)data[i]) << std::dec;
      }
    }
    std::cerr << "\", " << len << ")\n";
#endif  // TEST_LOGGING
  }

  void OnControl(const FlipControlFrame* frame) {
    FlipHeaderBlock headers;
    bool parsed_headers = false;
    switch (frame->type()) {
      case SYN_STREAM:
        parsed_headers = framer_.ParseHeaderBlock(
            reinterpret_cast<const FlipFrame*>(frame), &headers);
        DCHECK(parsed_headers);
        syn_frame_count_++;
        break;
      case SYN_REPLY:
        parsed_headers = framer_.ParseHeaderBlock(
            reinterpret_cast<const FlipFrame*>(frame), &headers);
        DCHECK(parsed_headers);
        syn_reply_frame_count_++;
        break;
      case FIN_STREAM:
        fin_frame_count_++;
        break;
      default:
        DCHECK(false);  // Error!
    }
    if (frame->flags() & CONTROL_FLAG_FIN)
      fin_flag_count_++;
  }

  void OnLameDuck() {
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    framer_.set_enable_compression(false);
    framer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 &&
           framer_.error_code() == FlipFramer::FLIP_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % std::min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed = framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
      if (framer_.state() == FlipFramer::FLIP_DONE)
        framer_.Reset();
    }
  }

  FlipFramer framer_;
  // Counters from the visitor callbacks.
  int error_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int data_bytes_;
  int fin_frame_count_;  // The count of FIN_STREAM type frames received.
  int fin_flag_count_;  // The count of frames with the FIN flag set.
  int zero_length_data_frame_count_;  // The count of zero-length data frames.
};

// Test our protocol constants
TEST_F(FlipFramerTest, ProtocolConstants) {
  EXPECT_EQ(8u, sizeof(FlipFrame));
  EXPECT_EQ(8u, sizeof(FlipDataFrame));
  EXPECT_EQ(12u, sizeof(FlipControlFrame));
  EXPECT_EQ(16u, sizeof(FlipSynStreamControlFrame));
  EXPECT_EQ(16u, sizeof(FlipSynReplyControlFrame));
  EXPECT_EQ(16u, sizeof(FlipFinStreamControlFrame));
  EXPECT_EQ(1, SYN_STREAM);
  EXPECT_EQ(2, SYN_REPLY);
  EXPECT_EQ(3, FIN_STREAM);
}

// Test some of the protocol helper functions
TEST_F(FlipFramerTest, FrameStructs) {
  FlipFrame frame;
  memset(&frame, 0, sizeof(frame));
  frame.set_length(12345);
  frame.set_flags(10);
  EXPECT_EQ(12345u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_EQ(false, frame.is_control_frame());

  memset(&frame, 0, sizeof(frame));
  frame.set_length(0);
  frame.set_flags(10);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_EQ(false, frame.is_control_frame());
}

TEST_F(FlipFramerTest, DataFrameStructs) {
  FlipDataFrame data_frame;
  memset(&data_frame, 0, sizeof(data_frame));
  data_frame.set_stream_id(12345);
  EXPECT_EQ(12345u, data_frame.stream_id());
}

TEST_F(FlipFramerTest, ControlFrameStructs) {
  FlipFramer framer;
  FlipHeaderBlock headers;

  scoped_array<FlipSynStreamControlFrame> syn_frame(
      framer.CreateSynStream(123, 2, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_EQ(kFlipProtocolVersion, syn_frame.get()->version());
  EXPECT_EQ(true, syn_frame.get()->is_control_frame());
  EXPECT_EQ(SYN_STREAM, syn_frame.get()->type());
  EXPECT_EQ(123u, syn_frame.get()->stream_id());
  EXPECT_EQ(2u, syn_frame.get()->priority());
  EXPECT_EQ(2, syn_frame.get()->header_block_len());

  scoped_array<FlipSynReplyControlFrame> syn_reply(
      framer.CreateSynReply(123, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_EQ(kFlipProtocolVersion, syn_reply.get()->version());
  EXPECT_EQ(true, syn_reply.get()->is_control_frame());
  EXPECT_EQ(SYN_REPLY, syn_reply.get()->type());
  EXPECT_EQ(123u, syn_reply.get()->stream_id());
  EXPECT_EQ(2, syn_reply.get()->header_block_len());

  scoped_array<FlipFinStreamControlFrame> fin_frame(
      framer.CreateFinStream(123, 444));
  EXPECT_EQ(kFlipProtocolVersion, fin_frame.get()->version());
  EXPECT_EQ(true, fin_frame.get()->is_control_frame());
  EXPECT_EQ(FIN_STREAM, fin_frame.get()->type());
  EXPECT_EQ(123u, fin_frame.get()->stream_id());
  EXPECT_EQ(444u, fin_frame.get()->status());
  fin_frame.get()->set_status(555);
  EXPECT_EQ(555u, fin_frame.get()->status());
}

// Test that we can encode and decode a FlipHeaderBlock.
TEST_F(FlipFramerTest, HeaderBlock) {
  FlipHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "charlie";
  FlipFramer framer;

  // Encode the header block into a SynStream frame.
  scoped_array<FlipSynStreamControlFrame> frame(
      framer.CreateSynStream(1, 1, CONTROL_FLAG_NONE, true, &headers));
  EXPECT_TRUE(frame.get() != NULL);

  FlipHeaderBlock new_headers;
  FlipFrame* control_frame = reinterpret_cast<FlipFrame*>(frame.get());
  framer.ParseHeaderBlock(control_frame, &new_headers);

  EXPECT_EQ(headers.size(), new_headers.size());
  EXPECT_EQ(headers["alpha"], new_headers["alpha"]);
  EXPECT_EQ(headers["gamma"], new_headers["gamma"]);
}

TEST_F(FlipFramerTest, HeaderBlockBarfsOnOutOfOrderHeaders) {
  FlipFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | 1);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length.
  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt16(0);  // Priority.

  frame.WriteUInt16(2);  // Number of headers.
  FlipHeaderBlock::iterator it;
  frame.WriteString("gamma");
  frame.WriteString("gamma");
  frame.WriteString("alpha");
  frame.WriteString("alpha");
  // write the length
  frame.WriteUInt32ToOffset(4, frame.length() - sizeof(FlipFrame));

  FlipHeaderBlock new_headers;
  const FlipFrame* control_frame = frame.data();
  FlipFramer framer;
  framer.set_enable_compression(false);
  EXPECT_FALSE(framer.ParseHeaderBlock(control_frame, &new_headers));
}

TEST_F(FlipFramerTest, BasicCompression) {
  FlipHeaderBlock headers;
  headers["server"] = "FlipServer 1.0";
  headers["date"] = "Mon 12 Jan 2009 12:12:12 PST";
  headers["status"] = "200";
  headers["version"] = "HTTP/1.1";
  headers["content-type"] = "text/html";
  headers["content-length"] = "12";

  FlipFramer framer;
  scoped_array<FlipSynStreamControlFrame>
      frame1(framer.CreateSynStream(1, 1, CONTROL_FLAG_NONE, true, &headers));
  scoped_array<FlipSynStreamControlFrame>
      frame2(framer.CreateSynStream(1, 1, CONTROL_FLAG_NONE, true, &headers));

  // Expect the second frame to be more compact than the first.
  EXPECT_LE(frame2.get()->length(), frame1.get()->length());

  // Decompress the first frame
  scoped_array<FlipFrame> frame3(
      framer.DecompressFrame(reinterpret_cast<FlipFrame*>(frame1.get())));

  // Decompress the second frame
  scoped_array<FlipFrame> frame4(
      framer.DecompressFrame(reinterpret_cast<FlipFrame*>(frame2.get())));

  // Expect frames 3 & 4 to be the same.
  EXPECT_EQ(0,
      memcmp(frame3.get(), frame4.get(),
      sizeof(FlipFrame) + frame3.get()->length()));
}

TEST_F(FlipFramerTest, Basic) {
  const unsigned char input[] = {
    0x80, 0x01, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,

    0x80, 0x01, 0x00, 0x01,   // SYN Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,   // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
      0xde, 0xad, 0xbe, 0xef,

    0x80, 0x01, 0x00, 0x03,   // FIN on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,   // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, 0x01, 0x00, 0x03,   // FIN on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
  };

  TestFlipVisitor visitor;
  visitor.SimulateInFramer(input, sizeof(input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(24, visitor.data_bytes_);
  EXPECT_EQ(2, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
}

// Test that the FIN flag on a data frame signifies EOF.
TEST_F(FlipFramerTest, FinOnDataFrame) {
  const unsigned char input[] = {
    0x80, 0x01, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, 0x01, 0x00, 0x02,   // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
      0xde, 0xad, 0xbe, 0xef,
  };

  TestFlipVisitor visitor;
  visitor.SimulateInFramer(input, sizeof(input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.syn_reply_frame_count_);
  EXPECT_EQ(16, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
}

// Test that the FIN flag on a SYN reply frame signifies EOF.
TEST_F(FlipFramerTest, FinOnSynReplyFrame) {
  const unsigned char input[] = {
    0x80, 0x01, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, 0x01, 0x00, 0x02,   // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',
  };

  TestFlipVisitor visitor;
  visitor.SimulateInFramer(input, sizeof(input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
}

}  // namespace flip


