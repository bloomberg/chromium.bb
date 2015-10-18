// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_headers_stream.h"

#include "base/test/histogram_tester.h"
#include "net/quic/quic_utils.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_spdy_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/reliable_quic_stream_peer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using base::HistogramTester;
using base::StringPiece;
using std::ostream;
using std::string;
using std::vector;
using testing::ElementsAre;
using testing::InSequence;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;
using testing::_;

namespace net {
namespace test {
namespace {

// TODO(ckrasic):  this workaround is due to absence of std::initializer_list
const bool kFins[] = {false, true};

class MockVisitor : public SpdyFramerVisitorInterface {
 public:
  MOCK_METHOD1(OnError, void(SpdyFramer* framer));
  MOCK_METHOD3(OnDataFrameHeader, void(SpdyStreamId stream_id,
                                       size_t length,
                                       bool fin));
  MOCK_METHOD4(OnStreamFrameData, void(SpdyStreamId stream_id,
                                       const char* data,
                                       size_t len,
                                       bool fin));
  MOCK_METHOD2(OnStreamPadding, void(SpdyStreamId stream_id, size_t len));
  MOCK_METHOD3(OnControlFrameHeaderData, bool(SpdyStreamId stream_id,
                                              const char* header_data,
                                              size_t len));
  MOCK_METHOD5(OnSynStream, void(SpdyStreamId stream_id,
                                 SpdyStreamId associated_stream_id,
                                 SpdyPriority priority,
                                 bool fin,
                                 bool unidirectional));
  MOCK_METHOD2(OnSynReply, void(SpdyStreamId stream_id, bool fin));
  MOCK_METHOD2(OnRstStream, void(SpdyStreamId stream_id,
                                 SpdyRstStreamStatus status));
  MOCK_METHOD1(OnSettings, void(bool clear_persisted));
  MOCK_METHOD3(OnSetting, void(SpdySettingsIds id, uint8 flags, uint32 value));
  MOCK_METHOD0(OnSettingsAck, void());
  MOCK_METHOD0(OnSettingsEnd, void());
  MOCK_METHOD2(OnPing, void(SpdyPingId unique_id, bool is_ack));
  MOCK_METHOD2(OnGoAway, void(SpdyStreamId last_accepted_stream_id,
                              SpdyGoAwayStatus status));
  MOCK_METHOD7(OnHeaders,
               void(SpdyStreamId stream_id,
                    bool has_priority,
                    SpdyPriority priority,
                    SpdyStreamId parent_stream_id,
                    bool exclusive,
                    bool fin,
                    bool end));
  MOCK_METHOD2(OnWindowUpdate,
               void(SpdyStreamId stream_id, int delta_window_size));
  MOCK_METHOD2(OnCredentialFrameData, bool(const char* credential_data,
                                           size_t len));
  MOCK_METHOD1(OnBlocked, void(SpdyStreamId stream_id));
  MOCK_METHOD3(OnPushPromise, void(SpdyStreamId stream_id,
                                   SpdyStreamId promised_stream_id,
                                   bool end));
  MOCK_METHOD2(OnContinuation, void(SpdyStreamId stream_id, bool end));
  MOCK_METHOD3(OnAltSvc,
               void(SpdyStreamId stream_id,
                    StringPiece origin,
                    const SpdyAltSvcWireFormat::AlternativeServiceVector&
                        altsvc_vector));
  MOCK_METHOD2(OnUnknownFrame, bool(SpdyStreamId stream_id, int frame_type));
};

// Run all tests with each version, and client or server
struct TestParams {
  TestParams(QuicVersion version, Perspective perspective)
      : version(version), perspective(perspective) {}

  friend ostream& operator<<(ostream& os, const TestParams& p) {
    os << "{ version: " << QuicVersionToString(p.version);
    os << ", perspective: " << p.perspective << " }";
    return os;
  }

  QuicVersion version;
  Perspective perspective;
};

// Constructs various test permutations.
vector<TestParams> GetTestParams() {
  vector<TestParams> params;
  QuicVersionVector all_supported_versions = QuicSupportedVersions();
  for (const QuicVersion version : all_supported_versions) {
    params.push_back(TestParams(version, Perspective::IS_CLIENT));
    params.push_back(TestParams(version, Perspective::IS_SERVER));
  }
  return params;
}

class QuicHeadersStreamTest : public ::testing::TestWithParam<TestParams> {
 public:
  QuicHeadersStreamTest()
      : connection_(new StrictMock<MockConnection>(&helper_,
                                                   perspective(),
                                                   GetVersion())),
        session_(connection_),
        headers_stream_(QuicSpdySessionPeer::GetHeadersStream(&session_)),
        body_("hello world"),
        framer_(HTTP2),
        stream_frame_(kHeadersStreamId, /*fin=*/false, /*offset=*/0, "") {
    headers_[":version"]  = "HTTP/1.1";
    headers_[":status"] = "200 Ok";
    headers_["content-length"] = "11";
    framer_.set_visitor(&visitor_);
    EXPECT_EQ(version(), session_.connection()->version());
    EXPECT_TRUE(headers_stream_ != nullptr);
    VLOG(1) << GetParam();
    connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  }

  QuicConsumedData SaveIov(const QuicIOVector& data) {
    const iovec* iov = data.iov;
    int count = data.iov_count;
    for (int i = 0 ; i < count; ++i) {
      saved_data_.append(static_cast<char*>(iov[i].iov_base), iov[i].iov_len);
    }
    return QuicConsumedData(saved_data_.length(), false);
  }

  bool SaveHeaderData(const char* data, int len) {
    saved_header_data_.append(data, len);
    return true;
  }

  void SaveHeaderDataStringPiece(StringPiece data) {
    saved_header_data_.append(data.data(), data.length());
  }

  void WriteHeadersAndExpectSynStream(QuicStreamId stream_id,
                                      bool fin,
                                      QuicPriority priority) {
    WriteHeadersAndCheckData(stream_id, fin, priority, SYN_STREAM);
  }

  void WriteHeadersAndExpectSynReply(QuicStreamId stream_id,
                                     bool fin) {
    WriteHeadersAndCheckData(stream_id, fin, 0, SYN_REPLY);
  }

  void WriteHeadersAndCheckData(QuicStreamId stream_id,
                                bool fin,
                                QuicPriority priority,
                                SpdyFrameType type) {
    // Write the headers and capture the outgoing data
    EXPECT_CALL(session_, WritevData(kHeadersStreamId, _, _, false, _, nullptr))
        .WillOnce(WithArgs<1>(Invoke(this, &QuicHeadersStreamTest::SaveIov)));
    headers_stream_->WriteHeaders(stream_id, headers_, fin, priority, nullptr);

    // Parse the outgoing data and check that it matches was was written.
    if (type == SYN_STREAM) {
      EXPECT_CALL(visitor_, OnHeaders(stream_id, kHasPriority, priority,
                                      /*parent_stream_id=*/0,
                                      /*exclusive=*/false,

                                      fin, kFrameComplete));
    } else {
      EXPECT_CALL(visitor_,
                  OnHeaders(stream_id, !kHasPriority,
                            /*priority=*/0,
                            /*parent_stream_id=*/0,
                            /*exclusive=*/false, fin, kFrameComplete));
    }
    EXPECT_CALL(visitor_, OnControlFrameHeaderData(stream_id, _, _))
        .WillRepeatedly(WithArgs<1, 2>(
            Invoke(this, &QuicHeadersStreamTest::SaveHeaderData)));
    if (fin) {
      EXPECT_CALL(visitor_, OnStreamFrameData(stream_id, nullptr, 0, true));
    }
    framer_.ProcessInput(saved_data_.data(), saved_data_.length());
    EXPECT_FALSE(framer_.HasError())
        << SpdyFramer::ErrorCodeToString(framer_.error_code());

    CheckHeaders();
    saved_data_.clear();
  }

  void CheckHeaders() {
    SpdyHeaderBlock headers;
    EXPECT_EQ(saved_header_data_.length(),
              framer_.ParseHeaderBlockInBuffer(saved_header_data_.data(),
                                               saved_header_data_.length(),
                                               &headers));
    EXPECT_TRUE(CompareSpdyHeaderBlocks(headers_, headers));
    saved_header_data_.clear();
  }

  Perspective perspective() { return GetParam().perspective; }

  QuicVersion version() { return GetParam().version; }

  QuicVersionVector GetVersion() {
    QuicVersionVector versions;
    versions.push_back(version());
    return versions;
  }

  void CloseConnection() {
    QuicConnectionPeer::CloseConnection(connection_);
  }

  static const bool kFrameComplete = true;
  static const bool kHasPriority = true;

  MockHelper helper_;
  StrictMock<MockConnection>* connection_;
  StrictMock<MockQuicSpdySession> session_;
  QuicHeadersStream* headers_stream_;
  SpdyHeaderBlock headers_;
  string body_;
  string saved_data_;
  string saved_header_data_;
  SpdyFramer framer_;
  StrictMock<MockVisitor> visitor_;
  QuicStreamFrame stream_frame_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicHeadersStreamTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicHeadersStreamTest, StreamId) {
  EXPECT_EQ(3u, headers_stream_->id());
}

TEST_P(QuicHeadersStreamTest, EffectivePriority) {
  EXPECT_EQ(0u, headers_stream_->EffectivePriority());
}

TEST_P(QuicHeadersStreamTest, WriteHeaders) {
  for (QuicStreamId stream_id = kClientDataStreamId1;
       stream_id < kClientDataStreamId3; stream_id += 2) {
    for (bool fin : kFins) {
      if (perspective() == Perspective::IS_SERVER) {
        WriteHeadersAndExpectSynReply(stream_id, fin);
      } else {
        for (QuicPriority priority = 0; priority < 7; ++priority) {
          // TODO(rch): implement priorities correctly.
          WriteHeadersAndExpectSynStream(stream_id, fin, 0);
        }
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessRawData) {
  for (QuicStreamId stream_id = kClientDataStreamId1;
       stream_id < kClientDataStreamId3; stream_id += 2) {
    for (bool fin : kFins) {
      for (QuicPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        scoped_ptr<SpdySerializedFrame> frame;
        if (perspective() == Perspective::IS_SERVER) {
          SpdyHeadersIR headers_frame(stream_id);
          headers_frame.set_header_block(headers_);
          headers_frame.set_fin(fin);
          headers_frame.set_has_priority(true);
          frame.reset(framer_.SerializeFrame(headers_frame));
          EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0));
        } else {
          SpdyHeadersIR headers_frame(stream_id);
          headers_frame.set_header_block(headers_);
          headers_frame.set_fin(fin);
          frame.reset(framer_.SerializeFrame(headers_frame));
        }
        EXPECT_CALL(session_, OnStreamHeaders(stream_id, _))
            .WillRepeatedly(WithArgs<1>(
                Invoke(this,
                       &QuicHeadersStreamTest::SaveHeaderDataStringPiece)));
        EXPECT_CALL(session_,
                    OnStreamHeadersComplete(stream_id, fin, frame->size()));
        stream_frame_.data = StringPiece(frame->data(), frame->size());
        headers_stream_->OnStreamFrame(stream_frame_);
        stream_frame_.offset += frame->size();
        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, EmptyHeaderHOLBlockedTime) {
// In the absence of surfacing HOL measurements externally, via UMA
// or tcp connection stats, log messages are the only indication.
// This test verifies that false positives are not generated when
// headers arrive in order.
#if 0
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(_, _, _)).Times(0);
  log.StartCapturingLogs();
#endif
  InSequence seq;
  HistogramTester histogram_tester;
  bool fin = true;
  for (int stream_num = 0; stream_num < 10; stream_num++) {
    QuicStreamId stream_id = QuicClientDataStreamId(stream_num);
    // Replace with "WriteHeadersAndSaveData"
    scoped_ptr<SpdySerializedFrame> frame;
    if (perspective() == Perspective::IS_SERVER) {
      SpdyHeadersIR headers_frame(stream_id);
      headers_frame.set_header_block(headers_);
      headers_frame.set_fin(fin);
      headers_frame.set_has_priority(true);
      frame.reset(framer_.SerializeFrame(headers_frame));
      EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0));
    } else {
      SpdyHeadersIR headers_frame(stream_id);
      headers_frame.set_header_block(headers_);
      headers_frame.set_fin(fin);
      frame.reset(framer_.SerializeFrame(headers_frame));
    }
    EXPECT_CALL(session_, OnStreamHeaders(stream_id, _));
    EXPECT_CALL(session_,
                OnStreamHeadersComplete(stream_id, fin, frame->size()));
    stream_frame_.data = StringPiece(frame->data(), frame->size());
    headers_stream_->OnStreamFrame(stream_frame_);
    connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
    stream_frame_.offset += frame->size();
  }
  histogram_tester.ExpectTotalCount("Net.QuicSession.HeadersHOLBlockedTime", 0);
}

TEST_P(QuicHeadersStreamTest, NonEmptyHeaderHOLBlockedTime) {
// In the absence of surfacing HOL measurements externally, via UMA
// or tcp connection stats, log messages are the only indication.
// This test verifies that HOL blocking log messages are correct
// when there are out of order arrivals.
#if 0
  ScopedMockLog log(kDoNotCaptureLogsYet);
#endif
  HistogramTester histogram_tester;
  QuicStreamId stream_id;
  bool fin = true;
  QuicStreamFrame stream_frames[10];
  scoped_ptr<SpdySerializedFrame> frames[10];
  // First create all the frames in order
  {
    InSequence seq;
    for (int stream_num = 0; stream_num < 10; ++stream_num) {
      stream_id = QuicClientDataStreamId(stream_num);
      if (perspective() == Perspective::IS_SERVER) {
        SpdyHeadersIR headers_frame(stream_id);
        headers_frame.set_header_block(headers_);
        headers_frame.set_fin(fin);
        headers_frame.set_has_priority(true);
        frames[stream_num].reset(framer_.SerializeFrame(headers_frame));
        EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0)).Times(1);
      } else {
        SpdyHeadersIR headers_frame(stream_id);
        headers_frame.set_header_block(headers_);
        headers_frame.set_fin(fin);
        frames[stream_num].reset(framer_.SerializeFrame(headers_frame));
      }
      stream_frames[stream_num] = stream_frame_;
      stream_frames[stream_num].data =
          StringPiece(frames[stream_num]->data(), frames[stream_num]->size());
      DVLOG(1) << "make frame for stream " << stream_num << " offset "
               << stream_frames[stream_num].offset;
      stream_frame_.offset += frames[stream_num]->size();
      EXPECT_CALL(session_, OnStreamHeaders(stream_id, _)).Times(1);
      EXPECT_CALL(session_, OnStreamHeadersComplete(stream_id, fin, _))
          .Times(1);
    }
  }
#if 0
  // Actually writing the frames in reverse order will trigger log messages.
  {
    InSequence seq;
    for (int stream_num = 0; stream_num < 10; ++stream_num) {
      stream_id = QuicClientDataStreamId(stream_num);
      if (stream_num > 0) {
        string expected_msg = StringPrintf(
            "stream %d: Net.QuicSession.HeadersHOLBlockedTime %d",
            stream_id, stream_num);
#ifndef NDEBUG
        EXPECT_CALL(log, Log(INFO, _, expected_msg));
#endif
      }
    }
  }
  log.StartCapturingLogs();
#endif
  for (int stream_num = 9; stream_num >= 0; --stream_num) {
    DVLOG(1) << "OnStreamFrame for stream " << stream_num << " offset "
             << stream_frames[stream_num].offset;
    headers_stream_->OnStreamFrame(stream_frames[stream_num]);
    connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  }
  // We expect 1 sample each for delays from 1 to 9 ms (8 and 9 go
  // into the same bucket).
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Net.QuicSession.HeadersHOLBlockedTime"),
      ElementsAre(Bucket(1, 1), Bucket(2, 1), Bucket(3, 1), Bucket(4, 1),
                  Bucket(5, 1), Bucket(6, 1), Bucket(7, 1), Bucket(8, 2)));
}

TEST_P(QuicHeadersStreamTest, ProcessLargeRawData) {
  // We want to create a frame that is more than the SPDY Framer's max control
  // frame size, which is 16K, but less than the HPACK decoders max decode
  // buffer size, which is 32K.
  headers_["key0"] = string(1 << 13, '.');
  headers_["key1"] = string(1 << 13, '.');
  headers_["key2"] = string(1 << 13, '.');
  for (QuicStreamId stream_id = kClientDataStreamId1;
       stream_id < kClientDataStreamId3; stream_id += 2) {
    for (bool fin : kFins) {
      for (QuicPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        scoped_ptr<SpdySerializedFrame> frame;
        if (perspective() == Perspective::IS_SERVER) {
          SpdyHeadersIR headers_frame(stream_id);
          headers_frame.set_header_block(headers_);
          headers_frame.set_fin(fin);
          headers_frame.set_has_priority(true);
          frame.reset(framer_.SerializeFrame(headers_frame));
          EXPECT_CALL(session_, OnStreamHeadersPriority(stream_id, 0));
        } else {
          SpdyHeadersIR headers_frame(stream_id);
          headers_frame.set_header_block(headers_);
          headers_frame.set_fin(fin);
          frame.reset(framer_.SerializeFrame(headers_frame));
        }
        EXPECT_CALL(session_, OnStreamHeaders(stream_id, _))
            .WillRepeatedly(WithArgs<1>(Invoke(
                this, &QuicHeadersStreamTest::SaveHeaderDataStringPiece)));
        EXPECT_CALL(session_,
                    OnStreamHeadersComplete(stream_id, fin, frame->size()));
        stream_frame_.data = StringPiece(frame->data(), frame->size());
        headers_stream_->OnStreamFrame(stream_frame_);
        stream_frame_.offset += frame->size();
        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessBadData) {
  const char kBadData[] = "blah blah blah";
  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(
                                QUIC_INVALID_HEADERS_STREAM_DATA, _))
      .Times(::testing::AnyNumber());
  stream_frame_.data = StringPiece(kBadData, strlen(kBadData));
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyDataFrame) {
  SpdyDataIR data(2, "");
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY DATA frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  stream_frame_.data = StringPiece(frame->data(), frame->size());
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyRstStreamFrame) {
  SpdyRstStreamIR data(2, RST_STREAM_PROTOCOL_ERROR, "");
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  "SPDY RST_STREAM frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  stream_frame_.data = StringPiece(frame->data(), frame->size());
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdySettingsFrame) {
  SpdySettingsIR data;
  data.AddSetting(SETTINGS_HEADER_TABLE_SIZE, true, true, 0);
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  "SPDY SETTINGS frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  stream_frame_.data = StringPiece(frame->data(), frame->size());
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyPingFrame) {
  SpdyPingIR data(1);
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY PING frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  stream_frame_.data = StringPiece(frame->data(), frame->size());
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyGoAwayFrame) {
  SpdyGoAwayIR data(1, GOAWAY_PROTOCOL_ERROR, "go away");
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY GOAWAY frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  stream_frame_.data = StringPiece(frame->data(), frame->size());
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyWindowUpdateFrame) {
  SpdyWindowUpdateIR data(1, 1);
  scoped_ptr<SpdySerializedFrame> frame(framer_.SerializeFrame(data));
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  "SPDY WINDOW_UPDATE frame received."))
      .WillOnce(InvokeWithoutArgs(this,
                                  &QuicHeadersStreamTest::CloseConnection));
  stream_frame_.data = StringPiece(frame->data(), frame->size());
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, NoConnectionLevelFlowControl) {
  EXPECT_FALSE(ReliableQuicStreamPeer::StreamContributesToConnectionFlowControl(
      headers_stream_));
}

}  // namespace
}  // namespace test
}  // namespace net
