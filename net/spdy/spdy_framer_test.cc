// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>
#include <limits>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/spdy/mock_spdy_framer_visitor.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

using base::StringPiece;
using std::string;
using std::max;
using std::min;
using std::numeric_limits;
using testing::_;

namespace net {

namespace test {

static const size_t kMaxDecompressedSize = 1024;

class MockDebugVisitor : public SpdyFramerDebugVisitorInterface {
 public:
  MOCK_METHOD4(OnSendCompressedFrame, void(SpdyStreamId stream_id,
                                           SpdyFrameType type,
                                           size_t payload_len,
                                           size_t frame_len));

  MOCK_METHOD3(OnReceiveCompressedFrame, void(SpdyStreamId stream_id,
                                              SpdyFrameType type,
                                              size_t frame_len));
};

class SpdyFramerTestUtil {
 public:
  // Decompress a single frame using the decompression context held by
  // the SpdyFramer.  The implemention is meant for use only in tests
  // and will CHECK fail if the input is anything other than a single,
  // well-formed compressed frame.
  //
  // Returns a new decompressed SpdyFrame.
  template<class SpdyFrameType> static SpdyFrame* DecompressFrame(
      SpdyFramer* framer, const SpdyFrameType& frame) {
    DecompressionVisitor visitor(framer->protocol_version());
    framer->set_visitor(&visitor);
    CHECK_EQ(frame.size(), framer->ProcessInput(frame.data(), frame.size()));
    CHECK_EQ(SpdyFramer::SPDY_RESET, framer->state());
    framer->set_visitor(NULL);

    char* buffer = visitor.ReleaseBuffer();
    CHECK(buffer != NULL);
    SpdyFrame* decompressed_frame = new SpdyFrame(buffer, visitor.size(), true);
    if (framer->protocol_version() == 4) {
      SetFrameLength(decompressed_frame,
                     visitor.size(),
                     framer->protocol_version());
    } else {
      SetFrameLength(decompressed_frame,
                     visitor.size() - framer->GetControlFrameHeaderSize(),
                     framer->protocol_version());
    }
    return decompressed_frame;
  }

  class DecompressionVisitor : public SpdyFramerVisitorInterface {
   public:
    explicit DecompressionVisitor(SpdyMajorVersion version)
        : version_(version), size_(0), finished_(false) {}

    void ResetBuffer() {
      CHECK(buffer_.get() == NULL);
      CHECK_EQ(0u, size_);
      CHECK(!finished_);
      buffer_.reset(new char[kMaxDecompressedSize]);
    }

    virtual void OnError(SpdyFramer* framer) OVERRIDE { LOG(FATAL); }
    virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                   size_t length,
                                   bool fin) OVERRIDE {
      LOG(FATAL) << "Unexpected data frame header";
    }
    virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                   const char* data,
                                   size_t len,
                                   bool fin) OVERRIDE {
      LOG(FATAL);
    }

    virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                          const char* header_data,
                                          size_t len) OVERRIDE {
      CHECK(buffer_.get() != NULL);
      CHECK_GE(kMaxDecompressedSize, size_ + len);
      CHECK(!finished_);
      if (len != 0) {
        memcpy(buffer_.get() + size_, header_data, len);
        size_ += len;
      } else {
        // Done.
        finished_ = true;
      }
      return true;
    }

    virtual void OnSynStream(SpdyStreamId stream_id,
                             SpdyStreamId associated_stream_id,
                             SpdyPriority priority,
                             bool fin,
                             bool unidirectional) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdySynStreamIR syn_stream(stream_id);
      syn_stream.set_associated_to_stream_id(associated_stream_id);
      syn_stream.set_priority(priority);
      syn_stream.set_fin(fin);
      syn_stream.set_unidirectional(unidirectional);
      scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetSynStreamMinimumSize());
      size_ += framer.GetSynStreamMinimumSize();
    }

    virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdyHeadersIR headers(stream_id);
      headers.set_fin(fin);
      scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetHeadersMinimumSize());
      size_ += framer.GetSynStreamMinimumSize();
    }

    virtual void OnRstStream(SpdyStreamId stream_id,
                             SpdyRstStreamStatus status) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnSetting(SpdySettingsIds id,
                           uint8 flags,
                           uint32 value) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnSettingsEnd() OVERRIDE { LOG(FATAL); }
    virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                          SpdyGoAwayStatus status) OVERRIDE {
      LOG(FATAL);
    }

    virtual void OnHeaders(SpdyStreamId stream_id, bool fin) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdyHeadersIR headers(stream_id);
      headers.set_fin(fin);
      scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetHeadersMinimumSize());
      size_ += framer.GetHeadersMinimumSize();
    }

    virtual void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) {
      LOG(FATAL);
    }

    virtual void OnPushPromise(SpdyStreamId stream_id,
                               SpdyStreamId promised_stream_id) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdyPushPromiseIR push_promise(stream_id, promised_stream_id);
      scoped_ptr<SpdyFrame> frame(framer.SerializePushPromise(push_promise));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetPushPromiseMinimumSize());
      size_ += framer.GetPushPromiseMinimumSize();
    }

    char* ReleaseBuffer() {
      CHECK(finished_);
      return buffer_.release();
    }

    virtual void OnWindowUpdate(SpdyStreamId stream_id,
                                uint32 delta_window_size) OVERRIDE {
      LOG(FATAL);
    }

    size_t size() const {
      CHECK(finished_);
      return size_;
    }

   private:
    SpdyMajorVersion version_;
    scoped_ptr<char[]> buffer_;
    size_t size_;
    bool finished_;

    DISALLOW_COPY_AND_ASSIGN(DecompressionVisitor);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyFramerTestUtil);
};

class TestSpdyVisitor : public SpdyFramerVisitorInterface,
                        public SpdyFramerDebugVisitorInterface {
 public:
  static const size_t kDefaultHeaderBufferSize = 16 * 1024 * 1024;

  explicit TestSpdyVisitor(SpdyMajorVersion version)
    : framer_(version),
      use_compression_(false),
      error_count_(0),
      syn_frame_count_(0),
      syn_reply_frame_count_(0),
      headers_frame_count_(0),
      goaway_count_(0),
      setting_count_(0),
      settings_ack_sent_(0),
      settings_ack_received_(0),
      last_window_update_stream_(0),
      last_window_update_delta_(0),
      last_push_promise_stream_(0),
      last_push_promise_promised_stream_(0),
      data_bytes_(0),
      fin_frame_count_(0),
      fin_opaque_data_(),
      fin_flag_count_(0),
      zero_length_data_frame_count_(0),
      control_frame_header_data_count_(0),
      zero_length_control_frame_header_data_count_(0),
      data_frame_count_(0),
      last_payload_len_(0),
      last_frame_len_(0),
      header_buffer_(new char[kDefaultHeaderBufferSize]),
      header_buffer_length_(0),
      header_buffer_size_(kDefaultHeaderBufferSize),
      header_stream_id_(-1),
      header_control_type_(DATA),
      header_buffer_valid_(false) {
  }

  virtual void OnError(SpdyFramer* f) OVERRIDE {
    LOG(INFO) << "SpdyFramer Error: "
              << SpdyFramer::ErrorCodeToString(f->error_code());
    error_count_++;
  }

  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE {
    data_frame_count_++;
    header_stream_id_ = stream_id;
  }

  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE {
    EXPECT_EQ(header_stream_id_, stream_id);
    if (len == 0)
      ++zero_length_data_frame_count_;

    data_bytes_ += len;
    std::cerr << "OnStreamFrameData(" << stream_id << ", \"";
    if (len > 0) {
      for (size_t i = 0 ; i < len; ++i) {
        std::cerr << std::hex << (0xFF & (unsigned int)data[i]) << std::dec;
      }
    }
    std::cerr << "\", " << len << ")\n";
  }

  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE {
    ++control_frame_header_data_count_;
    CHECK_EQ(header_stream_id_, stream_id);
    if (len == 0) {
      ++zero_length_control_frame_header_data_count_;
      // Indicates end-of-header-block.
      CHECK(header_buffer_valid_);
      size_t parsed_length = framer_.ParseHeaderBlockInBuffer(
          header_buffer_.get(), header_buffer_length_, &headers_);
      DCHECK_EQ(header_buffer_length_, parsed_length);
      return true;
    }
    const size_t available = header_buffer_size_ - header_buffer_length_;
    if (len > available) {
      header_buffer_valid_ = false;
      return false;
    }
    memcpy(header_buffer_.get() + header_buffer_length_, header_data, len);
    header_buffer_length_ += len;
    return true;
  }

  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional) OVERRIDE {
    syn_frame_count_++;
    InitHeaderStreaming(SYN_STREAM, stream_id);
    if (fin) {
      fin_flag_count_++;
    }
  }

  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {
    syn_reply_frame_count_++;
    InitHeaderStreaming(SYN_REPLY, stream_id);
    if (fin) {
      fin_flag_count_++;
    }
  }

  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {
    fin_frame_count_++;
  }

  virtual bool OnRstStreamFrameData(const char* rst_stream_data,
                                    size_t len) OVERRIDE {
    if ((rst_stream_data != NULL) && (len > 0)) {
      fin_opaque_data_ += std::string(rst_stream_data, len);
    }
    return true;
  }

  virtual void OnSetting(SpdySettingsIds id,
                         uint8 flags,
                         uint32 value) OVERRIDE {
    setting_count_++;
  }

  virtual void OnSettingsAck() OVERRIDE {
    DCHECK_GE(4, framer_.protocol_version());
    settings_ack_received_++;
  }

  virtual void OnSettingsEnd() OVERRIDE {
    if (framer_.protocol_version() < 4) { return; }
    settings_ack_sent_++;
  }

  virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE {
    DLOG(FATAL);
  }

  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {
    goaway_count_++;
  }

  virtual void OnHeaders(SpdyStreamId stream_id, bool fin) OVERRIDE {
    headers_frame_count_++;
    InitHeaderStreaming(HEADERS, stream_id);
    if (fin) {
      fin_flag_count_++;
    }
  }

  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE {
    last_window_update_stream_ = stream_id;
    last_window_update_delta_ = delta_window_size;
  }

  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id) OVERRIDE {
    InitHeaderStreaming(PUSH_PROMISE, stream_id);
    last_push_promise_stream_ = stream_id;
    last_push_promise_promised_stream_ = promised_stream_id;
  }

  virtual void OnSendCompressedFrame(SpdyStreamId stream_id,
                                     SpdyFrameType type,
                                     size_t payload_len,
                                     size_t frame_len) OVERRIDE {
    last_payload_len_ = payload_len;
    last_frame_len_ = frame_len;
  }

  virtual void OnReceiveCompressedFrame(SpdyStreamId stream_id,
                                        SpdyFrameType type,
                                        size_t frame_len) OVERRIDE {
    last_frame_len_ = frame_len;
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    framer_.set_enable_compression(use_compression_);
    framer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 &&
           framer_.error_code() == SpdyFramer::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed = framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
    }
  }

  void InitHeaderStreaming(SpdyFrameType header_control_type,
                           SpdyStreamId stream_id) {
    DCHECK_GE(header_control_type, FIRST_CONTROL_TYPE);
    DCHECK_LE(header_control_type, LAST_CONTROL_TYPE);
    memset(header_buffer_.get(), 0, header_buffer_size_);
    header_buffer_length_ = 0;
    header_stream_id_ = stream_id;
    header_control_type_ = header_control_type;
    header_buffer_valid_ = true;
    DCHECK_NE(header_stream_id_, SpdyFramer::kInvalidStream);
  }

  // Override the default buffer size (16K). Call before using the framer!
  void set_header_buffer_size(size_t header_buffer_size) {
    header_buffer_size_ = header_buffer_size;
    header_buffer_.reset(new char[header_buffer_size]);
  }

  static size_t header_data_chunk_max_size() {
    return SpdyFramer::kHeaderDataChunkMaxSize;
  }

  SpdyFramer framer_;
  bool use_compression_;

  // Counters from the visitor callbacks.
  int error_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int headers_frame_count_;
  int goaway_count_;
  int setting_count_;
  int settings_ack_sent_;
  int settings_ack_received_;
  SpdyStreamId last_window_update_stream_;
  uint32 last_window_update_delta_;
  SpdyStreamId last_push_promise_stream_;
  SpdyStreamId last_push_promise_promised_stream_;
  int data_bytes_;
  int fin_frame_count_;  // The count of RST_STREAM type frames received.
  std::string fin_opaque_data_;
  int fin_flag_count_;  // The count of frames with the FIN flag set.
  int zero_length_data_frame_count_;  // The count of zero-length data frames.
  int control_frame_header_data_count_;  // The count of chunks received.
  // The count of zero-length control frame header data chunks received.
  int zero_length_control_frame_header_data_count_;
  int data_frame_count_;
  size_t last_payload_len_;
  size_t last_frame_len_;

  // Header block streaming state:
  scoped_ptr<char[]> header_buffer_;
  size_t header_buffer_length_;
  size_t header_buffer_size_;
  SpdyStreamId header_stream_id_;
  SpdyFrameType header_control_type_;
  bool header_buffer_valid_;
  SpdyHeaderBlock headers_;
};

// Retrieves serialized headers from SYN_STREAM frame.
// Does not check that the given frame is a SYN_STREAM.
base::StringPiece GetSerializedHeaders(const SpdyFrame* frame,
                                       const SpdyFramer& framer) {
  return base::StringPiece(frame->data() + framer.GetSynStreamMinimumSize(),
                           frame->size() - framer.GetSynStreamMinimumSize());
}

}  // namespace test

}  // namespace net

using net::test::SetFrameLength;
using net::test::SetFrameFlags;
using net::test::CompareCharArraysWithHexError;
using net::test::SpdyFramerTestUtil;
using net::test::TestSpdyVisitor;
using net::test::GetSerializedHeaders;

namespace net {

class SpdyFramerTest : public ::testing::TestWithParam<SpdyMajorVersion> {
 protected:
  virtual void SetUp() {
    spdy_version_ = GetParam();
    spdy_version_ch_ = static_cast<unsigned char>(spdy_version_);
  }

  void CompareFrame(const string& description,
                    const SpdyFrame& actual_frame,
                    const unsigned char* expected,
                    const int expected_len) {
    const unsigned char* actual =
        reinterpret_cast<const unsigned char*>(actual_frame.data());
    CompareCharArraysWithHexError(
        description, actual, actual_frame.size(), expected, expected_len);
  }

  void CompareFrames(const string& description,
                     const SpdyFrame& expected_frame,
                     const SpdyFrame& actual_frame) {
    CompareCharArraysWithHexError(
        description,
        reinterpret_cast<const unsigned char*>(expected_frame.data()),
        expected_frame.size(),
        reinterpret_cast<const unsigned char*>(actual_frame.data()),
        actual_frame.size());
  }

  // Returns true if the two header blocks have equivalent content.
  bool CompareHeaderBlocks(const SpdyHeaderBlock* expected,
                           const SpdyHeaderBlock* actual) {
    if (expected->size() != actual->size()) {
      LOG(ERROR) << "Expected " << expected->size() << " headers; actually got "
                 << actual->size() << ".";
      return false;
    }
    for (SpdyHeaderBlock::const_iterator it = expected->begin();
         it != expected->end();
         ++it) {
      SpdyHeaderBlock::const_iterator it2 = actual->find(it->first);
      if (it2 == actual->end()) {
        LOG(ERROR) << "Expected header name '" << it->first << "'.";
        return false;
      }
      if (it->second.compare(it2->second) != 0) {
        LOG(ERROR) << "Expected header named '" << it->first
                   << "' to have a value of '" << it->second
                   << "'. The actual value received was '" << it2->second
                   << "'.";
        return false;
      }
    }
    return true;
  }

  void AddSpdySettingFromWireFormat(SettingsMap* settings,
                                    uint32 key,
                                    uint32 value) {
    SettingsFlagsAndId flags_and_id =
        SettingsFlagsAndId::FromWireFormat(spdy_version_, key);
    SpdySettingsIds id = static_cast<SpdySettingsIds>(flags_and_id.id());
    SpdySettingsFlags flags =
        static_cast<SpdySettingsFlags>(flags_and_id.flags());
    CHECK(settings->find(id) == settings->end());
    settings->insert(std::make_pair(id, SettingsFlagsAndValue(flags, value)));
  }

  bool IsSpdy2() { return spdy_version_ == SPDY2; }
  bool IsSpdy3() { return spdy_version_ == SPDY3; }
  bool IsSpdy4() { return spdy_version_ == SPDY4; }

  // Version of SPDY protocol to be used.
  SpdyMajorVersion spdy_version_;
  unsigned char spdy_version_ch_;
};

// All tests are run with 3 different SPDY versions: SPDY/2, SPDY/3, SPDY/4.
INSTANTIATE_TEST_CASE_P(SpdyFramerTests,
                        SpdyFramerTest,
                        ::testing::Values(SPDY2, SPDY3, SPDY4));

// Test that we can encode and decode a SpdyHeaderBlock in serialized form.
TEST_P(SpdyFramerTest, HeaderBlockInBuffer) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  // Encode the header block into a SynStream frame.
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("alpha", "beta");
  syn_stream.SetHeader("gamma", "charlie");
  scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(frame.get() != NULL);
  base::StringPiece serialized_headers =
      GetSerializedHeaders(frame.get(), framer);
  SpdyHeaderBlock new_headers;
  EXPECT_TRUE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                              serialized_headers.size(),
                                              &new_headers));

  SpdyHeaderBlock headers = syn_stream.name_value_block();
  EXPECT_EQ(headers.size(), new_headers.size());
  EXPECT_EQ(headers["alpha"], new_headers["alpha"]);
  EXPECT_EQ(headers["gamma"], new_headers["gamma"]);
}

// Test that if there's not a full frame, we fail to parse it.
TEST_P(SpdyFramerTest, UndersizedHeaderBlockInBuffer) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  // Encode the header block into a SynStream frame.
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("alpha", "beta");
  syn_stream.SetHeader("gamma", "charlie");
  scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(frame.get() != NULL);

  base::StringPiece serialized_headers =
      GetSerializedHeaders(frame.get(), framer);
  SpdyHeaderBlock new_headers;
  EXPECT_FALSE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                               serialized_headers.size() - 2,
                                               &new_headers));
}

// Test that if we receive a SYN_REPLY with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, SynReplyWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdySynReplyIR syn_reply(0);
  syn_reply.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeSynReply(syn_reply));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

// Test that if we receive a HEADERS with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, HeadersWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdyHeadersIR headers_ir(0);
  headers_ir.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeHeaders(headers_ir));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

// Test that if we receive a PUSH_PROMISE with stream ID zero, we signal an
// error (but don't crash).
TEST_P(SpdyFramerTest, PushPromiseWithStreamIdZero) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdyPushPromiseIR push_promise(0, 4);
  push_promise.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(
      framer.SerializePushPromise(push_promise));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

// Test that if we receive a PUSH_PROMISE with promised stream ID zero, we
// signal an error (but don't crash).
TEST_P(SpdyFramerTest, PushPromiseWithPromisedStreamIdZero) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdyPushPromiseIR push_promise(3, 0);
  push_promise.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, DuplicateHeader) {
  SpdyFramer framer(spdy_version_);
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);
  if (spdy_version_ < 4) {
    frame.WriteControlFrameHeader(framer, SYN_STREAM, CONTROL_FLAG_NONE);
    frame.WriteUInt32(3);  // stream_id
    frame.WriteUInt32(0);  // associated stream id
    frame.WriteUInt16(0);  // Priority.
  } else {
    frame.WriteFramePrefix(framer, HEADERS, HEADERS_FLAG_PRIORITY, 3);
    frame.WriteUInt32(framer.GetHighestPriority());
  }

  if (IsSpdy2()) {
    frame.WriteUInt16(2);  // Number of headers.
    frame.WriteString("name");
    frame.WriteString("value1");
    frame.WriteString("name");
    frame.WriteString("value2");
  } else {
    frame.WriteUInt32(2);  // Number of headers.
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32("value1");
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32("value2");
  }
  // write the length
  frame.RewriteLength(framer);

  SpdyHeaderBlock new_headers;
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  base::StringPiece serialized_headers =
      GetSerializedHeaders(control_frame.get(), framer);
  // This should fail because duplicate headers are verboten by the spec.
  EXPECT_FALSE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                               serialized_headers.size(),
                                               &new_headers));
}

TEST_P(SpdyFramerTest, MultiValueHeader) {
  SpdyFramer framer(spdy_version_);
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);
  if (spdy_version_ < 4) {
    frame.WriteControlFrameHeader(framer, SYN_STREAM, CONTROL_FLAG_NONE);
    frame.WriteUInt32(3);  // stream_id
    frame.WriteUInt32(0);  // associated stream id
    frame.WriteUInt16(0);  // Priority.
  } else {
    frame.WriteFramePrefix(framer, HEADERS, HEADERS_FLAG_PRIORITY, 3);
    frame.WriteUInt32(framer.GetHighestPriority());
  }

  string value("value1\0value2");
  if (IsSpdy2()) {
    frame.WriteUInt16(1);  // Number of headers.
    frame.WriteString("name");
    frame.WriteString(value);
  } else {
    frame.WriteUInt32(1);  // Number of headers.
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32(value);
  }
  // write the length
  frame.RewriteLength(framer);

  SpdyHeaderBlock new_headers;
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  base::StringPiece serialized_headers =
      GetSerializedHeaders(control_frame.get(), framer);
  EXPECT_TRUE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                              serialized_headers.size(),
                                              &new_headers));
  EXPECT_TRUE(new_headers.find("name") != new_headers.end());
  EXPECT_EQ(value, new_headers.find("name")->second);
}

TEST_P(SpdyFramerTest, BasicCompression) {
  scoped_ptr<TestSpdyVisitor> visitor(new TestSpdyVisitor(spdy_version_));
  SpdyFramer framer(spdy_version_);
  framer.set_debug_visitor(visitor.get());
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("server", "SpdyServer 1.0");
  syn_stream.SetHeader("date", "Mon 12 Jan 2009 12:12:12 PST");
  syn_stream.SetHeader("status", "200");
  syn_stream.SetHeader("version", "HTTP/1.1");
  syn_stream.SetHeader("content-type", "text/html");
  syn_stream.SetHeader("content-length", "12");
  scoped_ptr<SpdyFrame> frame1(framer.SerializeSynStream(syn_stream));
  size_t uncompressed_size1 = visitor->last_payload_len_;
  size_t compressed_size1 =
      visitor->last_frame_len_ - framer.GetSynStreamMinimumSize();
  if (IsSpdy2()) {
    EXPECT_EQ(139u, uncompressed_size1);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(155u, compressed_size1);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(135u, compressed_size1);
#endif  // !defined(USE_SYSTEM_ZLIB)
  } else {
    EXPECT_EQ(165u, uncompressed_size1);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(181u, compressed_size1);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(117u, compressed_size1);
#endif  // !defined(USE_SYSTEM_ZLIB)
  }
  scoped_ptr<SpdyFrame> frame2(framer.SerializeSynStream(syn_stream));
  size_t uncompressed_size2 = visitor->last_payload_len_;
  size_t compressed_size2 =
      visitor->last_frame_len_ - framer.GetSynStreamMinimumSize();

  // Expect the second frame to be more compact than the first.
  EXPECT_LE(frame2->size(), frame1->size());

  // Decompress the first frame
  scoped_ptr<SpdyFrame> frame3(SpdyFramerTestUtil::DecompressFrame(
      &framer, *frame1.get()));

  // Decompress the second frame
  visitor.reset(new TestSpdyVisitor(spdy_version_));
  framer.set_debug_visitor(visitor.get());
  scoped_ptr<SpdyFrame> frame4(SpdyFramerTestUtil::DecompressFrame(
      &framer, *frame2.get()));
  size_t uncompressed_size4 =
      frame4->size() - framer.GetSynStreamMinimumSize();
  size_t compressed_size4 =
      visitor->last_frame_len_ - framer.GetSynStreamMinimumSize();
  if (IsSpdy2()) {
    EXPECT_EQ(139u, uncompressed_size4);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(149u, compressed_size4);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(101u, compressed_size4);
#endif  // !defined(USE_SYSTEM_ZLIB)
  } else {
    EXPECT_EQ(165u, uncompressed_size4);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(175u, compressed_size4);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(102u, compressed_size4);
#endif  // !defined(USE_SYSTEM_ZLIB)
  }

  EXPECT_EQ(uncompressed_size1, uncompressed_size2);
  EXPECT_EQ(uncompressed_size1, uncompressed_size4);
  EXPECT_EQ(compressed_size2, compressed_size4);

  // Expect frames 3 & 4 to be the same.
  CompareFrames("Uncompressed SYN_STREAM", *frame3, *frame4);

  // Expect frames 3 to be the same as a uncompressed frame created
  // from scratch.
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> uncompressed_frame(
      framer.SerializeSynStream(syn_stream));
  CompareFrames("Uncompressed SYN_STREAM", *frame3, *uncompressed_frame);
}

TEST_P(SpdyFramerTest, CompressEmptyHeaders) {
  // See crbug.com/172383
  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader("server", "SpdyServer 1.0");
  syn_stream.SetHeader("date", "Mon 12 Jan 2009 12:12:12 PST");
  syn_stream.SetHeader("status", "200");
  syn_stream.SetHeader("version", "HTTP/1.1");
  syn_stream.SetHeader("content-type", "text/html");
  syn_stream.SetHeader("content-length", "12");
  syn_stream.SetHeader("x-empty-header", "");

  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);
  scoped_ptr<SpdyFrame> frame1(framer.SerializeSynStream(syn_stream));
}

TEST_P(SpdyFramerTest, Basic) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x08,  // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x18,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x02, 'h', '2',
    0x00, 0x02, 'v', '2',
    0x00, 0x02, 'h', '3',
    0x00, 0x02, 'v', '3',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #3
    0x00, 0x00, 0x00, 0x0c,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
  };

  const unsigned char kV3Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x08,  // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x02,
    'h', '2',
    0x00, 0x00, 0x00, 0x02,
    'v', '2', 0x00, 0x00,
    0x00, 0x02, 'h', '3',
    0x00, 0x00, 0x00, 0x02,
    'v', '3',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #3
    0x00, 0x00, 0x00, 0x0e,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
  };

  const unsigned char kV4Input[] = {
    0x00, 0x1c, 0x08, 0x08,           // SYN_STREAM #1
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'h',  'h',  0x00, 0x00,
    0x00, 0x02, 'v',  'v',

    0x00, 0x24, 0x08, 0x00,           // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x02,
    'h',  '2',  0x00, 0x00,
    0x00, 0x02, 'v',  '2',
    0x00, 0x00, 0x00, 0x02,
    'h',  '3',  0x00, 0x00,
    0x00, 0x02, 'v', '3',

    0x00, 0x14, 0x00, 0x00,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x10, 0x08, 0x08,           // SYN Stream #3
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x10, 0x00, 0x00,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x03,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x0c, 0x00, 0x00,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x0c, 0x03, 0x00,           // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x08, 0x00, 0x00,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x03,

    0x00, 0x17, 0x03, 0x00,           // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x52, 0x45, 0x53, 0x45,           // opaque data
    0x54, 0x53, 0x54, 0x52,
    0x45, 0x41, 0x4d,
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  } else {
    visitor.SimulateInFramer(kV4Input, sizeof(kV4Input));
  }

  EXPECT_EQ(2, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(24, visitor.data_bytes_);

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.fin_frame_count_);

  if (IsSpdy4()) {
    base::StringPiece reset_stream = "RESETSTREAM";
    EXPECT_EQ(reset_stream, visitor.fin_opaque_data_);
  } else {
    EXPECT_TRUE(visitor.fin_opaque_data_.empty());
  }

  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(4, visitor.data_frame_count_);
  visitor.fin_opaque_data_.clear();
}

// Test that the FIN flag on a data frame signifies EOF.
TEST_P(SpdyFramerTest, FinOnDataFrame) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };
  const unsigned char kV3Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v',  'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a',  'a',  0x00, 0x00,
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };
  const unsigned char kV4Input[] = {
    0x00, 0x1c, 0x08, 0x08,           // SYN_STREAM #1
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'h',  'h',  0x00, 0x00,
    0x00, 0x02, 'v',  'v',

    0x00, 0x18, 0x08, 0x00,           // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a',  'a',  0x00, 0x00,
    0x00, 0x02, 'b', 'b',

    0x00, 0x14, 0x00, 0x00,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x0c, 0x00, 0x01,           // DATA on Stream #1, with FIN
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  } else {
    visitor.SimulateInFramer(kV4Input, sizeof(kV4Input));
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  if (IsSpdy4()) {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  }
  EXPECT_EQ(16, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(2, visitor.data_frame_count_);
}

// Test that the FIN flag on a SYN reply frame signifies EOF.
TEST_P(SpdyFramerTest, FinOnSynReplyFrame) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',
  };
  const unsigned char kV3Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a', 'a',   0x00, 0x00,
    0x00, 0x02, 'b', 'b',
  };
  const unsigned char kV4Input[] = {
    0x00, 0x1c, 0x08, 0x08,           // SYN_STREAM #1
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'h',  'h',  0x00, 0x00,
    0x00, 0x02, 'v',  'v',

    0x00, 0x18, 0x08, 0x01,           // SYN_REPLY #1, with FIN
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a', 'a',   0x00, 0x00,
    0x00, 0x02, 'b', 'b',
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  } else {
    visitor.SimulateInFramer(kV4Input, sizeof(kV4Input));
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  if (IsSpdy4()) {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  }
  EXPECT_EQ(0, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, HeaderCompression) {
  SpdyFramer send_framer(spdy_version_);
  SpdyFramer recv_framer(spdy_version_);

  send_framer.set_enable_compression(true);
  recv_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kHeader3[] = "header3";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";
  const char kValue3[] = "value3";

  // SYN_STREAM #1
  SpdyHeaderBlock block;
  block[kHeader1] = kValue1;
  block[kHeader2] = kValue2;
  SpdySynStreamIR syn_ir_1(1);
  syn_ir_1.set_name_value_block(block);
  scoped_ptr<SpdyFrame> syn_frame_1(send_framer.SerializeFrame(syn_ir_1));
  EXPECT_TRUE(syn_frame_1.get() != NULL);

  // SYN_STREAM #2
  block[kHeader3] = kValue3;
  SpdySynStreamIR syn_stream(3);
  syn_stream.set_name_value_block(block);
  scoped_ptr<SpdyFrame> syn_frame_2(send_framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(syn_frame_2.get() != NULL);

  // Now start decompressing
  scoped_ptr<SpdyFrame> decompressed;
  scoped_ptr<SpdyFrame> uncompressed;
  base::StringPiece serialized_headers;
  SpdyHeaderBlock decompressed_headers;

  // Decompress SYN_STREAM #1
  decompressed.reset(SpdyFramerTestUtil::DecompressFrame(
      &recv_framer, *syn_frame_1.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  serialized_headers = GetSerializedHeaders(decompressed.get(), send_framer);
  EXPECT_TRUE(recv_framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                                   serialized_headers.size(),
                                                   &decompressed_headers));
  EXPECT_EQ(2u, decompressed_headers.size());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);

  // Decompress SYN_STREAM #2
  decompressed.reset(SpdyFramerTestUtil::DecompressFrame(
      &recv_framer, *syn_frame_2.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  serialized_headers = GetSerializedHeaders(decompressed.get(), send_framer);
  decompressed_headers.clear();
  EXPECT_TRUE(recv_framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                                   serialized_headers.size(),
                                                   &decompressed_headers));
  EXPECT_EQ(3u, decompressed_headers.size());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);
  EXPECT_EQ(kValue3, decompressed_headers[kHeader3]);
}

// Verify we don't leak when we leave streams unclosed
TEST_P(SpdyFramerTest, UnclosedStreamDataCompressors) {
  SpdyFramer send_framer(spdy_version_);

  send_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader(kHeader1, kValue1);
  syn_stream.SetHeader(kHeader2, kValue2);
  scoped_ptr<SpdyFrame> syn_frame(send_framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(syn_frame.get() != NULL);

  StringPiece bytes = "this is a test test test test test!";
  net::SpdyDataIR data_ir(1, bytes);
  data_ir.set_fin(true);
  scoped_ptr<SpdyFrame> send_frame(send_framer.SerializeData(data_ir));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(syn_frame->data());
  visitor.SimulateInFramer(data, syn_frame->size());
  data = reinterpret_cast<const unsigned char*>(send_frame->data());
  visitor.SimulateInFramer(data, send_frame->size());

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(bytes.size(), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(1, visitor.data_frame_count_);
}

// Verify we can decompress the stream even if handed over to the
// framer 1 byte at a time.
TEST_P(SpdyFramerTest, UnclosedStreamDataCompressorsOneByteAtATime) {
  SpdyFramer send_framer(spdy_version_);

  send_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader(kHeader1, kValue1);
  syn_stream.SetHeader(kHeader2, kValue2);
  scoped_ptr<SpdyFrame> syn_frame(send_framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(syn_frame.get() != NULL);

  const char bytes[] = "this is a test test test test test!";
  net::SpdyDataIR data_ir(1, StringPiece(bytes, arraysize(bytes)));
  data_ir.set_fin(true);
  scoped_ptr<SpdyFrame> send_frame(send_framer.SerializeData(data_ir));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(syn_frame->data());
  for (size_t idx = 0; idx < syn_frame->size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }
  data = reinterpret_cast<const unsigned char*>(send_frame->data());
  for (size_t idx = 0; idx < send_frame->size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(arraysize(bytes), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(1, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, WindowUpdateFrame) {
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
      net::SpdyWindowUpdateIR(1, 0x12345678)));

  const char kDescription[] = "WINDOW_UPDATE frame, stream 1, delta 0x12345678";
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
    0x80, spdy_version_ch_, 0x00, 0x09,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x12, 0x34, 0x56, 0x78
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x0c, 0x09, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x12, 0x34, 0x56, 0x78
  };

  if (IsSpdy4()) {
     CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
  } else {
     CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
  }
}

TEST_P(SpdyFramerTest, CreateDataFrame) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "'hello' data frame, no FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x05,
      'h',  'e',  'l',  'l',
      'o'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0d, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
      'h',  'e',  'l',  'l',
      'o'
    };
    const char bytes[] = "hello";

    SpdyDataIR data_ir(1, StringPiece(bytes, strlen(bytes)));
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }

    SpdyDataIR data_header_ir(1);
    data_header_ir.SetDataShallow(base::StringPiece(bytes, strlen(bytes)));
    frame.reset(framer.SerializeDataFrameHeader(data_header_ir));
    CompareCharArraysWithHexError(
        kDescription,
        reinterpret_cast<const unsigned char*>(frame->data()),
        framer.GetDataFrameMinimumSize(),
        IsSpdy4() ? kV4FrameData : kV3FrameData,
        framer.GetDataFrameMinimumSize());
  }

  {
    const char kDescription[] = "Data frame with negative data byte, no FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      0xff
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x09, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0xff
    };
    net::SpdyDataIR data_ir(1, StringPiece("\xff", 1));
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "'hello' data frame, with FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0d, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      'h',  'e',  'l',  'l',
      'o'
    };
    net::SpdyDataIR data_ir(1, StringPiece("hello", 5));
    data_ir.set_fin(true);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "Empty data frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x08, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
    };
    net::SpdyDataIR data_ir(1, StringPiece());
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "Data frame with max stream ID";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x7f, 0xff, 0xff, 0xff,
      0x01, 0x00, 0x00, 0x05,
      'h',  'e',  'l',  'l',
      'o'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0d, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
      'h',  'e',  'l',  'l',
      'o'
    };
    net::SpdyDataIR data_ir(0x7fffffff, "hello");
    data_ir.set_fin(true);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  if (!IsSpdy4()) {
    // This test does not apply to SPDY 4 because the max frame size is smaller
    // than 4MB.
    const char kDescription[] = "Large data frame";
    const int kDataSize = 4 * 1024 * 1024;  // 4 MB
    const string kData(kDataSize, 'A');
    const unsigned char kFrameHeader[] = {
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x40, 0x00, 0x00,
    };

    const int kFrameSize = arraysize(kFrameHeader) + kDataSize;
    scoped_ptr<unsigned char[]> expected_frame_data(
        new unsigned char[kFrameSize]);
    memcpy(expected_frame_data.get(), kFrameHeader, arraysize(kFrameHeader));
    memset(expected_frame_data.get() + arraysize(kFrameHeader), 'A', kDataSize);

    net::SpdyDataIR data_ir(1, StringPiece(kData.data(), kData.size()));
    data_ir.set_fin(true);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    CompareFrame(kDescription, *frame, expected_frame_data.get(), kFrameSize);
  }
}

TEST_P(SpdyFramerTest, CreateSynStreamUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_STREAM frame, lowest pri, no FIN";

    const unsigned char kPri = IsSpdy2() ? 0xC0 : 0xE0;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x20,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      kPri, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x2a,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      kPri, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'b',
      'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x2c, 0x08, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b', 'a', 'r'
    };
    SpdySynStreamIR syn_stream(1);
    syn_stream.set_priority(framer.GetLowestPriority());
    syn_stream.SetHeader("bar", "foo");
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header name, highest pri, FIN, "
        "max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x27,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x29, 0x08, 0x09,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    SpdySynStreamIR syn_stream(0x7fffffff);
    syn_stream.set_associated_to_stream_id(0x7fffffff);
    syn_stream.set_priority(framer.GetHighestPriority());
    syn_stream.set_fin(true);
    syn_stream.SetHeader("", "foo");
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header val, high pri, FIN, "
        "max stream ID";

    const unsigned char kPri = IsSpdy2() ? 0x40 : 0x20;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      kPri, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x27,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      kPri, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x00
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x29, 0x08, 0x09,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    SpdySynStreamIR syn_stream(0x7fffffff);
    syn_stream.set_associated_to_stream_id(0x7fffffff);
    syn_stream.set_priority(1);
    syn_stream.set_fin(true);
    syn_stream.SetHeader("bar", "foo");
    syn_stream.SetHeader("foo", "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateSynStreamCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] =
        "SYN_STREAM frame, low pri, no FIN";

    const SpdyPriority priority = IsSpdy2() ? 2 : 4;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x80, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x37,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x80, 0x00, 0x38, 0xEA,
      0xE3, 0xC6, 0xA7, 0xC2,
      0x02, 0xE5, 0x0E, 0x50,
      0xC2, 0x4B, 0x4A, 0x04,
      0xE5, 0x0B, 0x66, 0x80,
      0x00, 0x4A, 0xCB, 0xCF,
      0x07, 0x08, 0x20, 0x10,
      0x95, 0x96, 0x9F, 0x0F,
      0xA2, 0x00, 0x02, 0x28,
      0x29, 0xB1, 0x08, 0x20,
      0x80, 0x00, 0x00, 0x00,
      0x00, 0xFF, 0xFF,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x39, 0x08, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x04,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    SpdySynStreamIR syn_stream(1);
    syn_stream.set_priority(priority);
    syn_stream.SetHeader("bar", "foo");
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateSynReplyUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x28, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    SpdySynReplyIR syn_reply(1);
    syn_reply.SetHeader("bar", "foo");
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header name, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x25, 0x08, 0x01,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    SpdySynReplyIR syn_reply(0x7fffffff);
    syn_reply.set_fin(true);
    syn_reply.SetHeader("", "foo");
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header val, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x25, 0x08, 0x01,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    SpdySynReplyIR syn_reply(0x7fffffff);
    syn_reply.set_fin(true);
    syn_reply.SetHeader("bar", "foo");
    syn_reply.SetHeader("foo", "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateSynReplyCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x31,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x35, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    SpdySynReplyIR syn_reply(1);
    syn_reply.SetHeader("bar", "foo");
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateRstStream) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "RST_STREAM frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0f, 0x03, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      0x52, 0x53, 0x54
    };
    net::SpdyRstStreamIR rst_stream(1, RST_STREAM_PROTOCOL_ERROR, "RST");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "RST_STREAM frame with max stream ID";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0c, 0x03, 0x00,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    net::SpdyRstStreamIR rst_stream(0x7FFFFFFF,
                                    RST_STREAM_PROTOCOL_ERROR,
                                    "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "RST_STREAM frame with max status code";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x06,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0c, 0x03, 0x00,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x06,
    };
    net::SpdyRstStreamIR rst_stream(0x7FFFFFFF,
                                    RST_STREAM_INTERNAL_ERROR,
                                    "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreateSettings) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "Network byte order SETTINGS frame";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x0c,
      0x00, 0x00, 0x00, 0x01,
      0x04, 0x03, 0x02, 0x01,
      0x0a, 0x0b, 0x0c, 0x0d,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x0c,
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x02, 0x03, 0x04,
      0x0a, 0x0b, 0x0c, 0x0d,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0d, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x01, 0x0a, 0x0b, 0x0c,
      0x0d,
    };

    uint32 kValue = 0x0a0b0c0d;
    SpdySettingsIR settings_ir;

    SpdySettingsFlags kFlags = static_cast<SpdySettingsFlags>(0x01);
    SpdySettingsIds kId = static_cast<SpdySettingsIds>(0x020304);
    if (IsSpdy4()) {
      kId = static_cast<SpdySettingsIds>(0x01);
    }
    SettingsMap settings;
    settings[kId] = SettingsFlagsAndValue(kFlags, kValue);
    EXPECT_EQ(kFlags, settings[kId].first);
    EXPECT_EQ(kValue, settings[kId].second);
    settings_ir.AddSetting(kId,
                           kFlags & SETTINGS_FLAG_PLEASE_PERSIST,
                           kFlags & SETTINGS_FLAG_PERSISTED,
                           kValue);

    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] = "Basic SETTINGS frame";

    SettingsMap settings;
    AddSpdySettingFromWireFormat(
        &settings, 0x00000000, 0x00000001);  // 1st Setting
    AddSpdySettingFromWireFormat(
        &settings, 0x01000001, 0x00000002);  // 2nd Setting
    AddSpdySettingFromWireFormat(
        &settings, 0x02000002, 0x00000003);  // 3rd Setting
    AddSpdySettingFromWireFormat(
        &settings, 0x03000003, 0xff000004);  // 4th Setting

    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,  // 1st Setting
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x01,  // 2nd Setting
      0x00, 0x00, 0x00, 0x02,
      0x02, 0x00, 0x00, 0x02,  // 3rd Setting
      0x00, 0x00, 0x00, 0x03,
      0x03, 0x00, 0x00, 0x03,  // 4th Setting
      0xff, 0x00, 0x00, 0x04,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x1c, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x01,                    // 1st Setting
      0x00, 0x00, 0x00, 0x01,
      0x02,                    // 2nd Setting
      0x00, 0x00, 0x00, 0x02,
      0x03,                    // 3rd Setting
      0x00, 0x00, 0x00, 0x03,
      0x04,                    // 4th Setting
      0xff, 0x00, 0x00, 0x04,
    };
    SpdySettingsIR settings_ir;
    if (!IsSpdy4()) {
      for (SettingsMap::const_iterator it = settings.begin();
           it != settings.end();
           ++it) {
        settings_ir.AddSetting(it->first,
                               it->second.first & SETTINGS_FLAG_PLEASE_PERSIST,
                               it->second.first & SETTINGS_FLAG_PERSISTED,
                               it->second.second);
      }
    } else {
      SpdySettingsIds kId = static_cast<SpdySettingsIds>(0x01);
      settings_ir.AddSetting(kId, 0, 0, 0x00000001);
      kId = static_cast<SpdySettingsIds>(0x02);
      settings_ir.AddSetting(kId, 0, 0, 0x00000002);
      kId = static_cast<SpdySettingsIds>(0x03);
      settings_ir.AddSetting(kId, 0, 0, 0x00000003);
      kId = static_cast<SpdySettingsIds>(0x04);
      settings_ir.AddSetting(kId, 0, 0, 0xff000004);
    }
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "Empty SETTINGS frame";

    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x08, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x00,
    };
    SpdySettingsIR settings_ir;
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreatePingFrame) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "PING frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x06,
      0x00, 0x00, 0x00, 0x04,
      0x12, 0x34, 0x56, 0x78,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x10, 0x06, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x12, 0x34, 0x56, 0x78,
      0x9a, 0xbc, 0xde, 0xff,
    };
    const unsigned char kV4FrameDataWithAck[] = {
      0x00, 0x10, 0x06, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x12, 0x34, 0x56, 0x78,
      0x9a, 0xbc, 0xde, 0xff,
    };
    scoped_ptr<SpdyFrame> frame;
    if (IsSpdy4()) {
      const SpdyPingId kPingId = 0x123456789abcdeffULL;
      SpdyPingIR ping_ir(kPingId);
      // Tests SpdyPingIR when the ping is not an ack.
      ASSERT_FALSE(ping_ir.is_ack());
      frame.reset(framer.SerializePing(ping_ir));
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));

      // Tests SpdyPingIR when the ping is an ack.
      ping_ir.set_is_ack(true);
      frame.reset(framer.SerializePing(ping_ir));
      CompareFrame(kDescription, *frame,
                   kV4FrameDataWithAck, arraysize(kV4FrameDataWithAck));

    } else {
      frame.reset(framer.SerializePing(SpdyPingIR(0x12345678ull)));
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreateGoAway) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "GOAWAY frame";
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,  // Stream Id
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00,  // Stream Id
      0x00, 0x00, 0x00, 0x00,  // Status
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x12, 0x07, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,  // Stream id
      0x00, 0x00, 0x00, 0x00,  // Status
      0x47, 0x41,              // Opaque Description
    };
    SpdyGoAwayIR goaway_ir(0, GOAWAY_OK, "GA");
    scoped_ptr<SpdyFrame> frame(framer.SerializeGoAway(goaway_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] = "GOAWAY frame with max stream ID, status";
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x7f, 0xff, 0xff, 0xff,  // Stream Id
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,  // Stream Id
      0x00, 0x00, 0x00, 0x02,  // Status
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x12, 0x07, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x7f, 0xff, 0xff, 0xff,  // Stream Id
      0x00, 0x00, 0x00, 0x02,  // Status
      0x47, 0x41,              // Opaque Description
    };
    SpdyGoAwayIR goaway_ir(0x7FFFFFFF, GOAWAY_INTERNAL_ERROR, "GA");
    scoped_ptr<SpdyFrame> frame(framer.SerializeGoAway(goaway_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreateHeadersUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x28, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    SpdyHeadersIR headers_ir(1);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header name, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x25, 0x08, 0x01,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    SpdyHeadersIR headers_ir(0x7fffffff);
    headers_ir.set_fin(true);
    headers_ir.SetHeader("", "foo");
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x25, 0x08, 0x01,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    SpdyHeadersIR headers_ir(0x7fffffff);
    headers_ir.set_fin(true);
     headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateHeadersCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x31,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x35, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff
    };
    SpdyHeadersIR headers_ir(1);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateWindowUpdate) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "WINDOW_UPDATE frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0c, 0x09, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(
        framer.SerializeWindowUpdate(net::SpdyWindowUpdateIR(1, 1)));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max stream ID";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0c, 0x09, 0x00,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
        net::SpdyWindowUpdateIR(0x7FFFFFFF, 1)));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max window delta";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0c, 0x09, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
        net::SpdyWindowUpdateIR(1, 0x7FFFFFFF)));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, SerializeBlocked) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  SpdyFramer framer(spdy_version_);

  const char kDescription[] = "BLOCKED frame";
  const unsigned char kFrameData[] = {
    0x00, 0x08, 0x0b, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };
  SpdyBlockedIR blocked_ir(0);
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeFrame(blocked_ir));
  CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
}

TEST_P(SpdyFramerTest, CreateBlocked) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  SpdyFramer framer(spdy_version_);

  const char kDescription[] = "BLOCKED frame";
  const SpdyStreamId kStreamId = 3;

  scoped_ptr<SpdySerializedFrame> frame_serialized(
      framer.SerializeBlocked(SpdyBlockedIR(kStreamId)));
  SpdyBlockedIR blocked_ir(kStreamId);
  scoped_ptr<SpdySerializedFrame> frame_created(
                                    framer.SerializeFrame(blocked_ir));

  CompareFrames(kDescription, *frame_serialized, *frame_created);
}

TEST_P(SpdyFramerTest, CreatePushPromiseUncompressed) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  const char kDescription[] = "PUSH_PROMISE frame";

  const unsigned char kFrameData[] = {
    0x00, 0x2C, 0x0C, 0x00,  // length = 44, type = 12, flags = 0
    0x00, 0x00, 0x00, 0x2A,  // stream id = 42
    0x00, 0x00, 0x00, 0x39,  // promised stream id = 57
    0x00, 0x00, 0x00, 0x02,  // start of uncompressed header block
    0x00, 0x00, 0x00, 0x03,
    'b',  'a',  'r',  0x00,
    0x00, 0x00, 0x03, 'f',
    'o',  'o',  0x00, 0x00,
    0x00, 0x03, 'f',  'o',
    'o',  0x00, 0x00, 0x00,
    0x03, 'b',  'a',  'r'    // end of uncompressed header block
  };

  SpdyPushPromiseIR push_promise(42, 57);
  push_promise.SetHeader("bar", "foo");
  push_promise.SetHeader("foo", "bar");
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
  CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
}

TEST_P(SpdyFramerTest, CreatePushPromiseCompressed) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  const char kDescription[] = "PUSH_PROMISE frame";

  const unsigned char kFrameData[] = {
    0x00, 0x39, 0x0C, 0x00,  // length = 57, type = 12, flags = 0
    0x00, 0x00, 0x00, 0x2A,  // stream id = 42
    0x00, 0x00, 0x00, 0x39,  // promised stream id = 57
    0x38, 0xea, 0xe3, 0xc6,  // start of compressed header block
    0xa7, 0xc2, 0x02, 0xe5,
    0x0e, 0x50, 0xc2, 0x4b,
    0x4a, 0x04, 0xe5, 0x0b,
    0x66, 0x80, 0x00, 0x4a,
    0xcb, 0xcf, 0x07, 0x08,
    0x20, 0x10, 0x95, 0x96,
    0x9f, 0x0f, 0xa2, 0x00,
    0x02, 0x28, 0x29, 0xb1,
    0x08, 0x20, 0x80, 0x00,
    0x00, 0x00, 0x00, 0xff,
    0xff                     // end of compressed header block
  };

  SpdyPushPromiseIR push_promise(42, 57);
  push_promise.SetHeader("bar", "foo");
  push_promise.SetHeader("foo", "bar");
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
  CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
}

TEST_P(SpdyFramerTest, ReadCompressedSynStreamHeaderBlock) {
  SpdyFramer framer(spdy_version_);
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("aa", "vv");
  syn_stream.SetHeader("bb", "ww");
  SpdyHeaderBlock headers = syn_stream.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedSynReplyHeaderBlock) {
  SpdyFramer framer(spdy_version_);
  SpdySynReplyIR syn_reply(1);
  syn_reply.SetHeader("alpha", "beta");
  syn_reply.SetHeader("gamma", "delta");
  SpdyHeaderBlock headers = syn_reply.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynReply(syn_reply));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  if (IsSpdy4()) {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  }
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlock) {
  SpdyFramer framer(spdy_version_);
  SpdyHeadersIR headers_ir(1);
  headers_ir.SetHeader("alpha", "beta");
  headers_ir.SetHeader("gamma", "delta");
  SpdyHeaderBlock headers = headers_ir.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeHeaders(headers_ir));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least twice.
  EXPECT_LE(2, visitor.control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlockWithHalfClose) {
  SpdyFramer framer(spdy_version_);
  SpdyHeadersIR headers_ir(1);
  headers_ir.set_fin(true);
  headers_ir.SetHeader("alpha", "beta");
  headers_ir.SetHeader("gamma", "delta");
  SpdyHeaderBlock headers = headers_ir.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeHeaders(headers_ir));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least twice.
  EXPECT_LE(2, visitor.control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ControlFrameAtMaxSizeLimit) {
  // First find the size of the header value in order to just reach the control
  // frame max size.
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("aa", "");
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  const size_t kBigValueSize =
      framer.GetControlFrameBufferMaxSize() - control_frame->size();

  // Create a frame at exatly that size.
  string big_value(kBigValueSize, 'x');
  syn_stream.SetHeader("aa", big_value.c_str());
  control_frame.reset(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(control_frame.get() != NULL);
  EXPECT_EQ(framer.GetControlFrameBufferMaxSize(), control_frame->size());

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_LT(kBigValueSize, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ControlFrameTooLarge) {
  // First find the size of the header value in order to just reach the control
  // frame max size.
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader("aa", "");
  syn_stream.set_priority(1);
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  const size_t kBigValueSize =
      framer.GetControlFrameBufferMaxSize() - control_frame->size() + 1;

  // Create a frame at exatly that size.
  string big_value(kBigValueSize, 'x');
  syn_stream.SetHeader("aa", big_value.c_str());
  control_frame.reset(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(control_frame.get() != NULL);
  EXPECT_EQ(framer.GetControlFrameBufferMaxSize() + 1,
            control_frame->size());

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_FALSE(visitor.header_buffer_valid_);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

// Check that the framer stops delivering header data chunks once the visitor
// declares it doesn't want any more. This is important to guard against
// "zip bomb" types of attacks.
TEST_P(SpdyFramerTest, ControlFrameMuchTooLarge) {
  const size_t kHeaderBufferChunks = 4;
  const size_t kHeaderBufferSize =
      TestSpdyVisitor::header_data_chunk_max_size() * kHeaderBufferChunks;
  const size_t kBigValueSize = kHeaderBufferSize * 2;
  string big_value(kBigValueSize, 'x');
  SpdyFramer framer(spdy_version_);
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.set_fin(true);
  syn_stream.SetHeader("aa", big_value.c_str());
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.set_header_buffer_size(kHeaderBufferSize);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_FALSE(visitor.header_buffer_valid_);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());

  // The framer should have stoped delivering chunks after the visitor
  // signaled "stop" by returning false from OnControlFrameHeaderData().
  //
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least kHeaderBufferChunks + 1.
  EXPECT_LE(kHeaderBufferChunks + 1,
            static_cast<unsigned>(visitor.control_frame_header_data_count_));
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  // The framer should not have sent half-close to the visitor.
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
}

TEST_P(SpdyFramerTest, DecompressCorruptHeaderBlock) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  // Construct a SYN_STREAM control frame without compressing the header block,
  // and have the framer try to decompress it. This will cause the framer to
  // deal with a decompression error.
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("aa", "alpha beta gamma delta");
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_DECOMPRESS_FAILURE, visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ControlFrameSizesAreValidated) {
  SpdyFramer framer(spdy_version_);
  // Create a GoAway frame that has a few extra bytes at the end.
  // We create enough overhead to overflow the framer's control frame buffer.
  ASSERT_GE(250u, SpdyFramer::kControlFrameBufferSize);
  const unsigned char length =  1 + SpdyFramer::kControlFrameBufferSize;
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
    0x80, spdy_version_ch_, 0x00, 0x07,
    0x00, 0x00, 0x00, length,
    0x00, 0x00, 0x00, 0x00,  // Stream ID
    0x00, 0x00, 0x00, 0x00,  // Status
  };

  // SPDY version 4 and up GOAWAY frames are only bound to a minimal length,
  // since it may carry opaque data. Verify that minimal length is tested.
  const unsigned char less_than_min_length =  framer.GetGoAwayMinimumSize() - 1;
  const unsigned char kV4FrameData[] = {
    0x00, static_cast<uint8>(less_than_min_length), 0x07, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,  // Stream Id
    0x00, 0x00, 0x00, 0x00,  // Status
  };
  const size_t pad_length =
      length + framer.GetControlFrameHeaderSize() -
      (IsSpdy4() ? sizeof(kV4FrameData) : sizeof(kV3FrameData));
  string pad('A', pad_length);
  TestSpdyVisitor visitor(spdy_version_);

  if (IsSpdy4()) {
    visitor.SimulateInFramer(kV4FrameData, sizeof(kV4FrameData));
  } else {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  }
  visitor.SimulateInFramer(
      reinterpret_cast<const unsigned char*>(pad.c_str()),
      pad.length());

  EXPECT_EQ(1, visitor.error_count_);  // This generated an error.
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(0, visitor.goaway_count_);  // Frame not parsed.
}

TEST_P(SpdyFramerTest, ReadZeroLenSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SpdySettingsIR settings_ir;
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSettings(settings_ir));
  SetFrameLength(control_frame.get(), 0, spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      framer.GetControlFrameHeaderSize());
  // Should generate an error, since zero-len settings frames are unsupported.
  EXPECT_EQ(1, visitor.error_count_);
}

// Tests handling of SETTINGS frames with invalid length.
TEST_P(SpdyFramerTest, ReadBogusLenSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SpdySettingsIR settings_ir;

  // Add a setting to pad the frame so that we don't get a buffer overflow when
  // calling SimulateInFramer() below.
  SettingsMap settings;
  settings[SETTINGS_UPLOAD_BANDWIDTH] =
      SettingsFlagsAndValue(SETTINGS_FLAG_PLEASE_PERSIST, 0x00000002);
  settings_ir.AddSetting(SETTINGS_UPLOAD_BANDWIDTH,
                         true,  // please persist
                         false,
                         0x00000002);
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSettings(settings_ir));
  const size_t kNewLength = 14;
  SetFrameLength(control_frame.get(), kNewLength, spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      framer.GetControlFrameHeaderSize() + kNewLength);
  // Should generate an error, since its not possible to have a
  // settings frame of length kNewLength.
  EXPECT_EQ(1, visitor.error_count_);
}

// Tests handling of SETTINGS frames larger than the frame buffer size.
TEST_P(SpdyFramerTest, ReadLargeSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SpdySettingsIR settings_ir;
  SettingsMap settings;

  SpdySettingsFlags flags = SETTINGS_FLAG_PLEASE_PERSIST;
  settings[SETTINGS_UPLOAD_BANDWIDTH] =
    SettingsFlagsAndValue(flags, 0x00000002);
  settings[SETTINGS_DOWNLOAD_BANDWIDTH] =
    SettingsFlagsAndValue(flags, 0x00000003);
  settings[SETTINGS_ROUND_TRIP_TIME] = SettingsFlagsAndValue(flags, 0x00000004);
  for (SettingsMap::const_iterator it = settings.begin();
       it != settings.end();
       ++it) {
    settings_ir.AddSetting(it->first,
                           it->second.first & SETTINGS_FLAG_PLEASE_PERSIST,
                           it->second.first & SETTINGS_FLAG_PERSISTED,
                           it->second.second);
  }

  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSettings(settings_ir));
  EXPECT_LT(SpdyFramer::kControlFrameBufferSize,
            control_frame->size());
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;

  // Read all at once.
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(3, visitor.setting_count_);
  if (spdy_version_ >= 4) {
    EXPECT_EQ(1, visitor.settings_ack_sent_);
  }

  // Read data in small chunks.
  size_t framed_data = 0;
  size_t unframed_data = control_frame->size();
  size_t kReadChunkSize = 5;  // Read five bytes at a time.
  while (unframed_data > 0) {
    size_t to_read = min(kReadChunkSize, unframed_data);
    visitor.SimulateInFramer(
        reinterpret_cast<unsigned char*>(control_frame->data() + framed_data),
        to_read);
    unframed_data -= to_read;
    framed_data += to_read;
  }
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(3 * 2, visitor.setting_count_);
  if (spdy_version_ >= 4) {
    EXPECT_EQ(2, visitor.settings_ack_sent_);
  }
}

// Tests handling of SETTINGS frame with duplicate entries.
TEST_P(SpdyFramerTest, ReadDuplicateSettings) {
  SpdyFramer framer(spdy_version_);

  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x01, 0x00, 0x00, 0x00,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03, 0x00, 0x00, 0x00,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x01,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x17, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x01,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2FrameData, sizeof(kV2FrameData));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  } else {
    visitor.SimulateInFramer(kV4FrameData, sizeof(kV4FrameData));
  }

  if (!IsSpdy4()) {
    EXPECT_EQ(1, visitor.setting_count_);
    EXPECT_EQ(1, visitor.error_count_);
  } else {
    // In SPDY 4+, duplicate settings are allowed;
    // each setting replaces the previous value for that setting.
    EXPECT_EQ(3, visitor.setting_count_);
    EXPECT_EQ(0, visitor.error_count_);
    EXPECT_EQ(1, visitor.settings_ack_sent_);
  }
}

// Tests handling of SETTINGS frame with entries out of order.
TEST_P(SpdyFramerTest, ReadOutOfOrderSettings) {
  SpdyFramer framer(spdy_version_);

  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x02, 0x00, 0x00, 0x00,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03, 0x00, 0x00, 0x00,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x02,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x01, 0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x17, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x02,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2FrameData, sizeof(kV2FrameData));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  } else {
    visitor.SimulateInFramer(kV4FrameData, sizeof(kV4FrameData));
  }

  if (!IsSpdy4()) {
    EXPECT_EQ(1, visitor.setting_count_);
    EXPECT_EQ(1, visitor.error_count_);
  } else {
    // In SPDY 4+, settings are allowed in any order.
    EXPECT_EQ(3, visitor.setting_count_);
    EXPECT_EQ(0, visitor.error_count_);
    // EXPECT_EQ(1, visitor.settings_ack_count_);
  }
}

TEST_P(SpdyFramerTest, ProcessSettingsAckFrame) {
  if (spdy_version_ < 4) {
    return;
  }
  SpdyFramer framer(spdy_version_);

  const unsigned char kFrameData[] = {
    0x00, 0x08, 0x04, 0x01,
    0x00, 0x00, 0x00, 0x00,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.setting_count_);
  EXPECT_EQ(1, visitor.settings_ack_received_);
}

TEST_P(SpdyFramerTest, ReadWindowUpdate) {
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.SerializeWindowUpdate(net::SpdyWindowUpdateIR(1, 2)));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1u, visitor.last_window_update_stream_);
  EXPECT_EQ(2u, visitor.last_window_update_delta_);
}

TEST_P(SpdyFramerTest, ReceiveCredentialFrame) {
  if (!IsSpdy3()) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x0A,
      0x00, 0x00, 0x00, 0x33,
      0x00, 0x03, 0x00, 0x00,
      0x00, 0x05, 'p',  'r',
      'o',  'o',  'f',  0x00,
      0x00, 0x00, 0x06, 'a',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0C, 'a',  'n',  'o',
      't',  'h',  'e',  'r',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0A, 'f',  'i',  'n',
      'a',  'l',  ' ',  'c',
      'e',  'r',  't',
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kV3FrameData, arraysize(kV3FrameData));
  EXPECT_EQ(0, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadCredentialFrameFollowedByAnotherFrame) {
  if (!IsSpdy3()) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x0A,
      0x00, 0x00, 0x00, 0x33,
      0x00, 0x03, 0x00, 0x00,
      0x00, 0x05, 'p',  'r',
      'o',  'o',  'f',  0x00,
      0x00, 0x00, 0x06, 'a',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0C, 'a',  'n',  'o',
      't',  'h',  'e',  'r',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0A, 'f',  'i',  'n',
      'a',  'l',  ' ',  'c',
      'e',  'r',  't',
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  string multiple_frame_data(reinterpret_cast<const char*>(kV3FrameData),
                             arraysize(kV3FrameData));
  scoped_ptr<SpdyFrame> control_frame(
      framer.SerializeWindowUpdate(net::SpdyWindowUpdateIR(1, 2)));
  multiple_frame_data.append(string(control_frame->data(),
                                    control_frame->size()));
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned const char*>(multiple_frame_data.data()),
      multiple_frame_data.length());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1u, visitor.last_window_update_stream_);
  EXPECT_EQ(2u, visitor.last_window_update_delta_);
}

TEST_P(SpdyFramerTest, ReadCompressedPushPromise) {
  if (spdy_version_ < 4) {
    return;
  }

  SpdyFramer framer(spdy_version_);
  SpdyPushPromiseIR push_promise(42, 57);
  push_promise.SetHeader("foo", "bar");
  push_promise.SetHeader("bar", "foofoo");
  SpdyHeaderBlock headers = push_promise.name_value_block();
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
  EXPECT_TRUE(frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(frame->data()),
      frame->size());
  EXPECT_EQ(42u, visitor.last_push_promise_stream_);
  EXPECT_EQ(57u, visitor.last_push_promise_promised_stream_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadGarbage) {
  SpdyFramer framer(spdy_version_);
  unsigned char garbage_frame[256];
  memset(garbage_frame, ~0, sizeof(garbage_frame));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(garbage_frame, sizeof(garbage_frame));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadGarbageWithValidVersion) {
  if (IsSpdy4()) {
    // Not valid for SPDY 4 since there is no version field.
    return;
  }
  SpdyFramer framer(spdy_version_);
  const unsigned char kFrameData[] = {
    0x80, spdy_version_ch_, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kFrameData, arraysize(kFrameData));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, SizesTest) {
  SpdyFramer framer(spdy_version_);
  EXPECT_EQ(8u, framer.GetDataFrameMinimumSize());
  if (IsSpdy4()) {
    EXPECT_EQ(8u, framer.GetSynReplyMinimumSize());
    EXPECT_EQ(12u, framer.GetRstStreamMinimumSize());
    EXPECT_EQ(8u, framer.GetSettingsMinimumSize());
    EXPECT_EQ(16u, framer.GetPingSize());
    EXPECT_EQ(16u, framer.GetGoAwayMinimumSize());
    EXPECT_EQ(8u, framer.GetHeadersMinimumSize());
    EXPECT_EQ(12u, framer.GetWindowUpdateSize());
    EXPECT_EQ(8u, framer.GetBlockedSize());
    EXPECT_EQ(12u, framer.GetPushPromiseMinimumSize());
    EXPECT_EQ(8u, framer.GetFrameMinimumSize());
    EXPECT_EQ(65535u, framer.GetFrameMaximumSize());
    EXPECT_EQ(65527u, framer.GetDataFrameMaximumPayload());
  } else {
    EXPECT_EQ(8u, framer.GetControlFrameHeaderSize());
    EXPECT_EQ(18u, framer.GetSynStreamMinimumSize());
    EXPECT_EQ(IsSpdy2() ? 14u : 12u, framer.GetSynReplyMinimumSize());
    EXPECT_EQ(16u, framer.GetRstStreamMinimumSize());
    EXPECT_EQ(12u, framer.GetSettingsMinimumSize());
    EXPECT_EQ(12u, framer.GetPingSize());
    EXPECT_EQ(IsSpdy2() ? 12u : 16u, framer.GetGoAwayMinimumSize());
    EXPECT_EQ(IsSpdy2() ? 14u : 12u, framer.GetHeadersMinimumSize());
    EXPECT_EQ(16u, framer.GetWindowUpdateSize());
    EXPECT_EQ(8u, framer.GetFrameMinimumSize());
    EXPECT_EQ(16777215u, framer.GetFrameMaximumSize());
    EXPECT_EQ(16777207u, framer.GetDataFrameMaximumPayload());
  }
}

TEST_P(SpdyFramerTest, StateToStringTest) {
  EXPECT_STREQ("ERROR",
               SpdyFramer::StateToString(SpdyFramer::SPDY_ERROR));
  EXPECT_STREQ("AUTO_RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_AUTO_RESET));
  EXPECT_STREQ("RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_RESET));
  EXPECT_STREQ("READING_COMMON_HEADER",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_READING_COMMON_HEADER));
  EXPECT_STREQ("CONTROL_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_PAYLOAD));
  EXPECT_STREQ("IGNORE_REMAINING_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_IGNORE_REMAINING_PAYLOAD));
  EXPECT_STREQ("FORWARD_STREAM_FRAME",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_FORWARD_STREAM_FRAME));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_SETTINGS_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_SETTINGS_FRAME_PAYLOAD));
  EXPECT_STREQ("UNKNOWN_STATE",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_SETTINGS_FRAME_PAYLOAD + 1));
}

TEST_P(SpdyFramerTest, ErrorCodeToStringTest) {
  EXPECT_STREQ("NO_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::SPDY_NO_ERROR));
  EXPECT_STREQ("INVALID_CONTROL_FRAME",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_CONTROL_FRAME));
  EXPECT_STREQ("CONTROL_PAYLOAD_TOO_LARGE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE));
  EXPECT_STREQ("ZLIB_INIT_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_ZLIB_INIT_FAILURE));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_UNSUPPORTED_VERSION));
  EXPECT_STREQ("DECOMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_DECOMPRESS_FAILURE));
  EXPECT_STREQ("COMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_COMPRESS_FAILURE));
  EXPECT_STREQ("SPDY_INVALID_DATA_FRAME_FLAGS",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS));
  EXPECT_STREQ("SPDY_INVALID_CONTROL_FRAME_FLAGS",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS));
  EXPECT_STREQ("UNKNOWN_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::LAST_ERROR));
}

TEST_P(SpdyFramerTest, StatusCodeToStringTest) {
  EXPECT_STREQ("INVALID",
               SpdyFramer::StatusCodeToString(RST_STREAM_INVALID));
  EXPECT_STREQ("PROTOCOL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_PROTOCOL_ERROR));
  EXPECT_STREQ("INVALID_STREAM",
               SpdyFramer::StatusCodeToString(RST_STREAM_INVALID_STREAM));
  EXPECT_STREQ("REFUSED_STREAM",
               SpdyFramer::StatusCodeToString(RST_STREAM_REFUSED_STREAM));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::StatusCodeToString(RST_STREAM_UNSUPPORTED_VERSION));
  EXPECT_STREQ("CANCEL",
               SpdyFramer::StatusCodeToString(RST_STREAM_CANCEL));
  EXPECT_STREQ("INTERNAL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_INTERNAL_ERROR));
  EXPECT_STREQ("FLOW_CONTROL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_FLOW_CONTROL_ERROR));
  EXPECT_STREQ("UNKNOWN_STATUS",
               SpdyFramer::StatusCodeToString(RST_STREAM_NUM_STATUS_CODES));
}

TEST_P(SpdyFramerTest, FrameTypeToStringTest) {
  EXPECT_STREQ("DATA",
               SpdyFramer::FrameTypeToString(DATA));
  EXPECT_STREQ("SYN_STREAM",
               SpdyFramer::FrameTypeToString(SYN_STREAM));
  EXPECT_STREQ("SYN_REPLY",
               SpdyFramer::FrameTypeToString(SYN_REPLY));
  EXPECT_STREQ("RST_STREAM",
               SpdyFramer::FrameTypeToString(RST_STREAM));
  EXPECT_STREQ("SETTINGS",
               SpdyFramer::FrameTypeToString(SETTINGS));
  EXPECT_STREQ("NOOP",
               SpdyFramer::FrameTypeToString(NOOP));
  EXPECT_STREQ("PING",
               SpdyFramer::FrameTypeToString(PING));
  EXPECT_STREQ("GOAWAY",
               SpdyFramer::FrameTypeToString(GOAWAY));
  EXPECT_STREQ("HEADERS",
               SpdyFramer::FrameTypeToString(HEADERS));
  EXPECT_STREQ("WINDOW_UPDATE",
               SpdyFramer::FrameTypeToString(WINDOW_UPDATE));
  EXPECT_STREQ("PUSH_PROMISE",
               SpdyFramer::FrameTypeToString(PUSH_PROMISE));
  EXPECT_STREQ("CREDENTIAL",
               SpdyFramer::FrameTypeToString(CREDENTIAL));
}

TEST_P(SpdyFramerTest, CatchProbableHttpResponse) {
  if (IsSpdy4()) {
    // TODO(hkhalil): catch probable HTTP response in SPDY 4?
    return;
  }
  {
    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput("HTTP/1.1", 8);
    EXPECT_TRUE(framer.probable_http_response());
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS, framer.error_code())
        << SpdyFramer::ErrorCodeToString(framer.error_code());
  }
  {
    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput("HTTP/1.0", 8);
    EXPECT_TRUE(framer.probable_http_response());
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS, framer.error_code())
        << SpdyFramer::ErrorCodeToString(framer.error_code());
  }
}

TEST_P(SpdyFramerTest, DataFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    net::SpdyDataIR data_ir(1, StringPiece("hello", 5));
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~DATA_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnDataFrameHeader(1, 5, flags & DATA_FLAG_FIN));
      EXPECT_CALL(visitor, OnStreamFrameData(_, _, 5, false));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~DATA_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SynStreamFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    testing::StrictMock<test::MockDebugVisitor> debug_visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);
    framer.set_debug_visitor(&debug_visitor);

    EXPECT_CALL(debug_visitor, OnSendCompressedFrame(8, SYN_STREAM, _, _));

    SpdySynStreamIR syn_stream(8);
    syn_stream.set_associated_to_stream_id(3);
    syn_stream.set_priority(1);
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    int set_flags = flags;
    if (IsSpdy4()) {
      // PRIORITY required for SYN_STREAM simulation.
      set_flags |= HEADERS_FLAG_PRIORITY;
    }
    SetFrameFlags(frame.get(), set_flags, spdy_version_);

    if (!IsSpdy4() &&
        flags & ~(CONTROL_FLAG_FIN | CONTROL_FLAG_UNIDIRECTIONAL)) {
      EXPECT_CALL(visitor, OnError(_));
    } else if (IsSpdy4() &&
               flags & ~(CONTROL_FLAG_FIN | HEADERS_FLAG_PRIORITY)) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(8, SYN_STREAM, _));
      if (IsSpdy4()) {
        EXPECT_CALL(visitor, OnSynStream(8, 0, 1, flags & CONTROL_FLAG_FIN,
                                         false));
      } else {
        EXPECT_CALL(visitor, OnSynStream(8, 3, 1, flags & CONTROL_FLAG_FIN,
                                         flags & CONTROL_FLAG_UNIDIRECTIONAL));
      }
      EXPECT_CALL(visitor, OnControlFrameHeaderData(8, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (!IsSpdy4() &&
        flags & ~(CONTROL_FLAG_FIN | CONTROL_FLAG_UNIDIRECTIONAL)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else if (IsSpdy4() &&
        flags & ~(CONTROL_FLAG_FIN | HEADERS_FLAG_PRIORITY)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SynReplyFrameFlags) {
  if (IsSpdy4()) {
    // Covered by HEADERS case.
    return;
  }
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdySynReplyIR syn_reply(37);
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSynReply(37, flags & CONTROL_FLAG_FIN));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(37, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, RstStreamFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    net::SpdyRstStreamIR rst_stream(13, RST_STREAM_CANCEL, "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnRstStream(13, RST_STREAM_CANCEL));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SettingsFrameFlagsOldFormat) {
  if (spdy_version_ >= 4) { return; }
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdySettingsIR settings_ir;
    settings_ir.AddSetting(SETTINGS_UPLOAD_BANDWIDTH,
                           false,
                           false,
                           54321);
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSettings(
          flags & SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS));
      EXPECT_CALL(visitor, OnSetting(SETTINGS_UPLOAD_BANDWIDTH,
                                     SETTINGS_FLAG_NONE, 54321));
      EXPECT_CALL(visitor, OnSettingsEnd());
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SettingsFrameFlags) {
  if (spdy_version_ < 4) { return; }
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdySettingsIR settings_ir;
    settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, 0, 0, 16);
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSettings(flags & SETTINGS_FLAG_ACK));
      EXPECT_CALL(visitor, OnSetting(SETTINGS_INITIAL_WINDOW_SIZE, 0, 16));
      EXPECT_CALL(visitor, OnSettingsEnd());
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~SETTINGS_FLAG_ACK) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else if (flags & SETTINGS_FLAG_ACK) {
      // The frame is invalid because ACK frames should have no payload.
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, GoawayFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyGoAwayIR goaway_ir(97, GOAWAY_OK, "test");
    scoped_ptr<SpdyFrame> frame(framer.SerializeGoAway(goaway_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnGoAway(97, GOAWAY_OK));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, HeadersFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    if (IsSpdy4() && flags & HEADERS_FLAG_PRIORITY) {
      // Covered by SYN_STREAM case.
      continue;
    }
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyHeadersIR headers_ir(57);
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnHeaders(57, flags & CONTROL_FLAG_FIN));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(57, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, PingFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.SerializePing(SpdyPingIR(42)));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (spdy_version_ >= SPDY4 &&
        flags == PING_FLAG_ACK) {
      EXPECT_CALL(visitor, OnPing(42, true));
    } else if (flags == 0) {
      EXPECT_CALL(visitor, OnPing(42, false));
    } else {
      EXPECT_CALL(visitor, OnError(_));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if ((spdy_version_ >= SPDY4 && flags == PING_FLAG_ACK) ||
        flags == 0) {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, WindowUpdateFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
        net::SpdyWindowUpdateIR(4, 1024)));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnWindowUpdate(4, 1024));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, PushPromiseFrameFlags) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockSpdyFramerVisitor> visitor;
    testing::StrictMock<net::test::MockDebugVisitor> debug_visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);
    framer.set_debug_visitor(&debug_visitor);

    EXPECT_CALL(debug_visitor, OnSendCompressedFrame(42, PUSH_PROMISE, _, _));

    SpdyPushPromiseIR push_promise(42, 57);
    push_promise.SetHeader("foo", "bar");
    scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(42, PUSH_PROMISE, _));
      EXPECT_CALL(visitor, OnPushPromise(42, 57));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(42, _, _))
          .WillRepeatedly(testing::Return(true));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, EmptySynStream) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  testing::StrictMock<test::MockDebugVisitor> debug_visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);
  framer.set_debug_visitor(&debug_visitor);

  EXPECT_CALL(debug_visitor, OnSendCompressedFrame(1, SYN_STREAM, _, _));

  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
  // Adjust size to remove the name/value block.
  if (IsSpdy4()) {
    SetFrameLength(
        frame.get(),
        framer.GetSynStreamMinimumSize(),
        spdy_version_);
  } else {
    SetFrameLength(
        frame.get(),
        framer.GetSynStreamMinimumSize() - framer.GetControlFrameHeaderSize(),
        spdy_version_);
  }

  EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(1, SYN_STREAM, _));
  EXPECT_CALL(visitor, OnSynStream(1, 0, 1, false, false));
  EXPECT_CALL(visitor, OnControlFrameHeaderData(1, NULL, 0));

  framer.ProcessInput(frame->data(), framer.GetSynStreamMinimumSize());
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, SettingsFlagsAndId) {
  const uint32 kId = 0x020304;
  const uint32 kFlags = 0x01;
  const uint32 kWireFormat = htonl(IsSpdy2() ? 0x04030201 : 0x01020304);

  SettingsFlagsAndId id_and_flags =
      SettingsFlagsAndId::FromWireFormat(spdy_version_, kWireFormat);
  EXPECT_EQ(kId, id_and_flags.id());
  EXPECT_EQ(kFlags, id_and_flags.flags());
  EXPECT_EQ(kWireFormat, id_and_flags.GetWireFormat(spdy_version_));
}

// Test handling of a RST_STREAM with out-of-bounds status codes.
TEST_P(SpdyFramerTest, RstStreamStatusBounds) {
  DCHECK_GE(0xff, RST_STREAM_NUM_STATUS_CODES);

  const unsigned char kV3RstStreamInvalid[] = {
    0x80, spdy_version_ch_, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, RST_STREAM_INVALID
  };
  const unsigned char kV4RstStreamInvalid[] = {
    0x00, 0x0c, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, RST_STREAM_INVALID
  };

  const unsigned char kV3RstStreamNumStatusCodes[] = {
    0x80, spdy_version_ch_, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, RST_STREAM_NUM_STATUS_CODES
  };
  const unsigned char kV4RstStreamNumStatusCodes[] = {
    0x00, 0x0c, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, RST_STREAM_NUM_STATUS_CODES
  };

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnRstStream(1, RST_STREAM_INVALID));
  if (IsSpdy4()) {
    framer.ProcessInput(reinterpret_cast<const char*>(kV4RstStreamInvalid),
                        arraysize(kV4RstStreamInvalid));
  } else {
    framer.ProcessInput(reinterpret_cast<const char*>(kV3RstStreamInvalid),
                        arraysize(kV3RstStreamInvalid));
  }
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());

  EXPECT_CALL(visitor, OnRstStream(1, RST_STREAM_INVALID));
  if (IsSpdy4()) {
    framer.ProcessInput(
        reinterpret_cast<const char*>(kV4RstStreamNumStatusCodes),
        arraysize(kV4RstStreamNumStatusCodes));
  } else {
    framer.ProcessInput(
        reinterpret_cast<const char*>(kV3RstStreamNumStatusCodes),
        arraysize(kV3RstStreamNumStatusCodes));
  }
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

// Tests handling of a GOAWAY frame with out-of-bounds stream ID.
TEST_P(SpdyFramerTest, GoAwayStreamIdBounds) {
  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x04,
    0xff, 0xff, 0xff, 0xff,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x08,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x10, 0x07, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
  };

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnGoAway(0x7fffffff, GOAWAY_OK));
  if (IsSpdy2()) {
    framer.ProcessInput(reinterpret_cast<const char*>(kV2FrameData),
                        arraysize(kV2FrameData));
  } else if (IsSpdy3()) {
    framer.ProcessInput(reinterpret_cast<const char*>(kV3FrameData),
                        arraysize(kV3FrameData));
  } else {
    framer.ProcessInput(reinterpret_cast<const char*>(kV4FrameData),
                        arraysize(kV4FrameData));
  }
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, OnBlocked) {
  if (spdy_version_ < SPDY4) {
    return;
  }

  const SpdyStreamId kStreamId = 0;

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnBlocked(kStreamId));

  SpdyBlockedIR blocked_ir(0);
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeFrame(blocked_ir));
  framer.ProcessInput(frame->data(), framer.GetBlockedSize());

  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

}  // namespace net
