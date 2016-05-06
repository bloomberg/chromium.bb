// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/buffered_spdy_framer.h"

#include "base/logging.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class TestBufferedSpdyVisitor : public BufferedSpdyFramerVisitorInterface {
 public:
  explicit TestBufferedSpdyVisitor(SpdyMajorVersion spdy_version)
      : buffered_spdy_framer_(spdy_version),
        error_count_(0),
        setting_count_(0),
        syn_frame_count_(0),
        syn_reply_frame_count_(0),
        headers_frame_count_(0),
        push_promise_frame_count_(0),
        goaway_count_(0),
        header_stream_id_(static_cast<SpdyStreamId>(-1)),
        promised_stream_id_(static_cast<SpdyStreamId>(-1)) {}

  void OnError(SpdyFramer::SpdyError error_code) override {
    VLOG(1) << "SpdyFramer Error: " << error_code;
    error_count_++;
  }

  void OnStreamError(SpdyStreamId stream_id,
                     const std::string& description) override {
    VLOG(1) << "SpdyFramer Error on stream: " << stream_id << " "
            << description;
    error_count_++;
  }

  void OnSynStream(SpdyStreamId stream_id,
                   SpdyStreamId associated_stream_id,
                   SpdyPriority priority,
                   bool fin,
                   bool unidirectional,
                   const SpdyHeaderBlock& headers) override {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    syn_frame_count_++;
    headers_ = headers;
  }

  void OnSynReply(SpdyStreamId stream_id,
                  bool fin,
                  const SpdyHeaderBlock& headers) override {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    syn_reply_frame_count_++;
    headers_ = headers;
  }

  void OnHeaders(SpdyStreamId stream_id,
                 bool has_priority,
                 SpdyPriority priority,
                 SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 const SpdyHeaderBlock& headers) override {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    headers_frame_count_++;
    headers_ = headers;
  }

  void OnDataFrameHeader(SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override {
    ADD_FAILURE() << "Unexpected OnDataFrameHeader call.";
  }

  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override {
    LOG(FATAL) << "Unexpected OnStreamFrameData call.";
  }

  void OnStreamEnd(SpdyStreamId stream_id) override {
    LOG(FATAL) << "Unexpected OnStreamEnd call.";
  }

  void OnStreamPadding(SpdyStreamId stream_id, size_t len) override {
    LOG(FATAL) << "Unexpected OnStreamPadding call.";
  }

  SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      SpdyStreamId stream_id) override {
    LOG(FATAL) << "Unexpected OnHeaderFrameStart call.";
    return nullptr;
  }

  void OnHeaderFrameEnd(SpdyStreamId stream_id, bool end_headers) override {
    LOG(FATAL) << "Unexpected OnHeaderFrameEnd call.";
  }

  void OnSettings(bool clear_persisted) override {}

  void OnSetting(SpdySettingsIds id, uint8_t flags, uint32_t value) override {
    setting_count_++;
  }

  void OnPing(SpdyPingId unique_id, bool is_ack) override {}

  void OnRstStream(SpdyStreamId stream_id,
                   SpdyRstStreamStatus status) override {}

  void OnGoAway(SpdyStreamId last_accepted_stream_id,
                SpdyGoAwayStatus status,
                base::StringPiece debug_data) override {
    goaway_count_++;
    goaway_last_accepted_stream_id_ = last_accepted_stream_id;
    goaway_status_ = status;
    goaway_debug_data_.assign(debug_data.data(), debug_data.size());
  }

  void OnDataFrameHeader(const SpdySerializedFrame* frame) {
    LOG(FATAL) << "Unexpected OnDataFrameHeader call.";
  }

  void OnRstStream(const SpdySerializedFrame& frame) {}
  void OnGoAway(const SpdySerializedFrame& frame) {}
  void OnPing(const SpdySerializedFrame& frame) {}
  void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) override {}

  void OnPushPromise(SpdyStreamId stream_id,
                     SpdyStreamId promised_stream_id,
                     const SpdyHeaderBlock& headers) override {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    push_promise_frame_count_++;
    promised_stream_id_ = promised_stream_id;
    EXPECT_NE(promised_stream_id_, SpdyFramer::kInvalidStream);
    headers_ = headers;
  }

  bool OnUnknownFrame(SpdyStreamId stream_id, int frame_type) override {
    return true;
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    buffered_spdy_framer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 &&
           buffered_spdy_framer_.error_code() == SpdyFramer::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % std::min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed =
          buffered_spdy_framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
    }
  }

  BufferedSpdyFramer buffered_spdy_framer_;

  // Counters from the visitor callbacks.
  int error_count_;
  int setting_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int headers_frame_count_;
  int push_promise_frame_count_;
  int goaway_count_;

  // Header block streaming state:
  SpdyStreamId header_stream_id_;
  SpdyStreamId promised_stream_id_;

  // Headers from OnSyn, OnSynReply, OnHeaders and OnPushPromise for
  // verification.
  SpdyHeaderBlock headers_;

  // OnGoAway parameters.
  SpdyStreamId goaway_last_accepted_stream_id_;
  SpdyGoAwayStatus goaway_status_;
  std::string goaway_debug_data_;
};

}  // namespace

class BufferedSpdyFramerTest
    : public PlatformTest,
      public ::testing::WithParamInterface<NextProto> {
 protected:
  SpdyMajorVersion spdy_version() {
    return NextProtoToSpdyMajorVersion(GetParam());
  }
};

INSTANTIATE_TEST_CASE_P(NextProto,
                        BufferedSpdyFramerTest,
                        testing::Values(kProtoSPDY31,
                                        kProtoHTTP2));

TEST_P(BufferedSpdyFramerTest, OnSetting) {
  SpdyFramer framer(spdy_version());
  SpdySettingsIR settings_ir;
  settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, false, false, 2);
  settings_ir.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, false, false, 3);
  SpdySerializedFrame control_frame(framer.SerializeSettings(settings_ir));
  TestBufferedSpdyVisitor visitor(spdy_version());

  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.setting_count_);
}

TEST_P(BufferedSpdyFramerTest, ReadSynStreamHeaderBlock) {
  if (spdy_version() > SPDY3) {
    // SYN_STREAM not supported in SPDY>3.
    return;
  }
  SpdyHeaderBlock headers;
  headers["aa"] = "vv";
  headers["bb"] = "ww";
  BufferedSpdyFramer framer(spdy_version());
  std::unique_ptr<SpdySerializedFrame> control_frame(
      framer.CreateSynStream(1,  // stream_id
                             0,  // associated_stream_id
                             1,  // priority
                             CONTROL_FLAG_NONE, &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  EXPECT_EQ(headers, visitor.headers_);
}

TEST_P(BufferedSpdyFramerTest, ReadSynReplyHeaderBlock) {
  if (spdy_version() > SPDY3) {
    // SYN_REPLY not supported in SPDY>3.
    return;
  }
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  BufferedSpdyFramer framer(spdy_version());
  std::unique_ptr<SpdySerializedFrame> control_frame(
      framer.CreateSynReply(1,  // stream_id
                            CONTROL_FLAG_NONE, &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  if (spdy_version() < HTTP2) {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  }
  EXPECT_EQ(headers, visitor.headers_);
}

TEST_P(BufferedSpdyFramerTest, ReadHeadersHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  BufferedSpdyFramer framer(spdy_version());
  std::unique_ptr<SpdySerializedFrame> control_frame(
      framer.CreateHeaders(1,  // stream_id
                           CONTROL_FLAG_NONE,
                           0,  // priority
                           &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  EXPECT_EQ(headers, visitor.headers_);
}

TEST_P(BufferedSpdyFramerTest, ReadPushPromiseHeaderBlock) {
  if (spdy_version() < HTTP2)
    return;
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  BufferedSpdyFramer framer(spdy_version());
  std::unique_ptr<SpdySerializedFrame> control_frame(
      framer.CreatePushPromise(1, 2, &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_EQ(headers, visitor.headers_);
  EXPECT_EQ(1u, visitor.header_stream_id_);
  EXPECT_EQ(2u, visitor.promised_stream_id_);
}

TEST_P(BufferedSpdyFramerTest, GoAwayDebugData) {
  if (spdy_version() < HTTP2)
    return;
  BufferedSpdyFramer framer(spdy_version());
  std::unique_ptr<SpdySerializedFrame> goaway_frame(
      framer.CreateGoAway(2u, GOAWAY_FRAME_SIZE_ERROR, "foo"));

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(goaway_frame.get()->data()),
      goaway_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.goaway_count_);
  EXPECT_EQ(2u, visitor.goaway_last_accepted_stream_id_);
  EXPECT_EQ(GOAWAY_FRAME_SIZE_ERROR, visitor.goaway_status_);
  EXPECT_EQ("foo", visitor.goaway_debug_data_);
}

}  // namespace net
