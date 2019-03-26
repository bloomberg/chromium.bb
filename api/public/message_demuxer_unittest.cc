// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/message_demuxer.h"

#include "api/impl/testing/fake_clock.h"
#include "api/public/testing/message_demuxer_test_support.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "third_party/tinycbor/src/src/cbor.h"

namespace openscreen {
namespace {

using ::testing::_;
using ::testing::Invoke;

ErrorOr<size_t> ConvertDecodeResult(ssize_t result) {
  if (result < 0) {
    if (result == -CborErrorUnexpectedEOF)
      return Error::Code::kCborIncompleteMessage;
    else
      return Error::Code::kCborParsing;
  } else {
    return result;
  }
}

class MessageDemuxerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(
        msgs::EncodePresentationConnectionOpenRequest(request_, &buffer_));
  }

  void ExpectDecodedRequest(
      ssize_t decode_result,
      const msgs::PresentationConnectionOpenRequest& received_request) {
    ASSERT_GT(decode_result, 0);
    EXPECT_EQ(decode_result, static_cast<ssize_t>(buffer_.size() - 1));
    EXPECT_EQ(request_.request_id, received_request.request_id);
    EXPECT_EQ(request_.presentation_id, received_request.presentation_id);
    EXPECT_EQ(request_.connection_id, received_request.connection_id);
  }

  const uint64_t endpoint_id_ = 13;
  const uint64_t connection_id_ = 45;
  FakeClock fake_clock_{
      platform::Clock::time_point(std::chrono::milliseconds(1298424))};
  msgs::CborEncodeBuffer buffer_;
  msgs::PresentationConnectionOpenRequest request_{1, "fry-am-the-egg-man", 3};
  MockMessageCallback mock_callback_;
  MessageDemuxer demuxer_{FakeClock::now, MessageDemuxer::kDefaultBufferLimit};
};

}  // namespace

TEST_F(MessageDemuxerTest, WatchStartStop) {
  MessageDemuxer::MessageWatch watch = demuxer_.WatchMessageType(
      endpoint_id_, msgs::Type::kPresentationConnectionOpenRequest,
      &mock_callback_);
  ASSERT_TRUE(watch);

  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _, _)).Times(0);
  demuxer_.OnStreamData(endpoint_id_ + 1, 14, buffer_.data(), buffer_.size());

  msgs::PresentationConnectionOpenRequest received_request;
  ssize_t decode_result = 0;
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size());
  ExpectDecodedRequest(decode_result, received_request);

  watch = MessageDemuxer::MessageWatch();
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _, _)).Times(0);
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size());
}

TEST_F(MessageDemuxerTest, BufferPartialMessage) {
  MockMessageCallback mock_callback_;
  constexpr uint64_t endpoint_id_ = 13;

  MessageDemuxer::MessageWatch watch = demuxer_.WatchMessageType(
      endpoint_id_, msgs::Type::kPresentationConnectionOpenRequest,
      &mock_callback_);
  ASSERT_TRUE(watch);

  msgs::PresentationConnectionOpenRequest received_request;
  ssize_t decode_result = 0;
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .Times(2)
      .WillRepeatedly(Invoke([&decode_result, &received_request](
                                 uint64_t endpoint_id, uint64_t connection_id,
                                 msgs::Type message_type, const uint8_t* buffer,
                                 size_t buffer_size,
                                 platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size() - 3);
  demuxer_.OnStreamData(endpoint_id_, connection_id_,
                        buffer_.data() + buffer_.size() - 3, 3);
  ExpectDecodedRequest(decode_result, received_request);
}

TEST_F(MessageDemuxerTest, DefaultWatch) {
  MockMessageCallback mock_callback_;
  constexpr uint64_t endpoint_id_ = 13;

  MessageDemuxer::MessageWatch watch = demuxer_.SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionOpenRequest, &mock_callback_);
  ASSERT_TRUE(watch);

  msgs::PresentationConnectionOpenRequest received_request;
  ssize_t decode_result = 0;
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size());
  ExpectDecodedRequest(decode_result, received_request);
}

TEST_F(MessageDemuxerTest, DefaultWatchOverridden) {
  MockMessageCallback mock_callback_global;
  MockMessageCallback mock_callback_;
  constexpr uint64_t endpoint_id_ = 13;

  MessageDemuxer::MessageWatch default_watch =
      demuxer_.SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationConnectionOpenRequest,
          &mock_callback_global);
  ASSERT_TRUE(default_watch);
  MessageDemuxer::MessageWatch watch = demuxer_.WatchMessageType(
      endpoint_id_, msgs::Type::kPresentationConnectionOpenRequest,
      &mock_callback_);
  ASSERT_TRUE(watch);

  msgs::PresentationConnectionOpenRequest received_request;
  ssize_t decode_result = 0;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      mock_callback_global,
      OnStreamMessage(endpoint_id_ + 1, 14,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  demuxer_.OnStreamData(endpoint_id_ + 1, 14, buffer_.data(), buffer_.size());
  ExpectDecodedRequest(decode_result, received_request);

  decode_result = 0;
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size());
  ExpectDecodedRequest(decode_result, received_request);
}

TEST_F(MessageDemuxerTest, WatchAfterData) {
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size());

  msgs::PresentationConnectionOpenRequest received_request;
  ssize_t decode_result = 0;
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  MessageDemuxer::MessageWatch watch = demuxer_.WatchMessageType(
      endpoint_id_, msgs::Type::kPresentationConnectionOpenRequest,
      &mock_callback_);
  ASSERT_TRUE(watch);
  ExpectDecodedRequest(decode_result, received_request);
}

TEST_F(MessageDemuxerTest, WatchAfterMultipleData) {
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size());

  msgs::CborEncodeBuffer buffer;
  msgs::PresentationInitiationRequest request;
  request.request_id = 2;
  request.url = "https://example.com/recv";
  request.connection_id = 98;
  request.has_connection_id = true;
  ASSERT_TRUE(msgs::EncodePresentationInitiationRequest(request, &buffer));
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer.data(),
                        buffer.size());

  MockMessageCallback mock_init_callback;
  msgs::PresentationConnectionOpenRequest received_request;
  msgs::PresentationInitiationRequest received_init_request;
  ssize_t decode_result1 = 0;
  ssize_t decode_result2 = 0;
  MessageDemuxer::MessageWatch init_watch = demuxer_.WatchMessageType(
      endpoint_id_, msgs::Type::kPresentationInitiationRequest,
      &mock_init_callback);
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result1, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result1 = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result1);
      }));
  EXPECT_CALL(
      mock_init_callback,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationInitiationRequest, _, _, _))
      .WillOnce(Invoke([&decode_result2, &received_init_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result2 = msgs::DecodePresentationInitiationRequest(
            buffer, buffer_size, &received_init_request);
        return ConvertDecodeResult(decode_result2);
      }));
  MessageDemuxer::MessageWatch watch = demuxer_.WatchMessageType(
      endpoint_id_, msgs::Type::kPresentationConnectionOpenRequest,
      &mock_callback_);
  ASSERT_TRUE(watch);
  ExpectDecodedRequest(decode_result1, received_request);

  ASSERT_GT(decode_result2, 0);
  EXPECT_EQ(decode_result2, static_cast<ssize_t>(buffer.size() - 1));
  EXPECT_EQ(request.request_id, received_init_request.request_id);
  EXPECT_EQ(request.url, received_init_request.url);
  EXPECT_EQ(request.connection_id, received_init_request.connection_id);
  EXPECT_EQ(request.has_connection_id, received_init_request.has_connection_id);
}

TEST_F(MessageDemuxerTest, GlobalWatchAfterData) {
  demuxer_.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                        buffer_.size());

  msgs::PresentationConnectionOpenRequest received_request;
  ssize_t decode_result = 0;
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  MessageDemuxer::MessageWatch watch = demuxer_.SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionOpenRequest, &mock_callback_);
  ASSERT_TRUE(watch);
  ExpectDecodedRequest(decode_result, received_request);
}

TEST_F(MessageDemuxerTest, BufferLimit) {
  MessageDemuxer demuxer(FakeClock::now, 10);

  demuxer.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                       buffer_.size());
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _, _)).Times(0);
  MessageDemuxer::MessageWatch watch = demuxer.WatchMessageType(
      endpoint_id_, msgs::Type::kPresentationConnectionOpenRequest,
      &mock_callback_);

  msgs::PresentationConnectionOpenRequest received_request;
  ssize_t decode_result = 0;
  EXPECT_CALL(
      mock_callback_,
      OnStreamMessage(endpoint_id_, connection_id_,
                      msgs::Type::kPresentationConnectionOpenRequest, _, _, _))
      .WillOnce(Invoke([&decode_result, &received_request](
                           uint64_t endpoint_id, uint64_t connection_id,
                           msgs::Type message_type, const uint8_t* buffer,
                           size_t buffer_size,
                           platform::Clock::time_point now) {
        decode_result = msgs::DecodePresentationConnectionOpenRequest(
            buffer, buffer_size, &received_request);
        return ConvertDecodeResult(decode_result);
      }));
  demuxer.OnStreamData(endpoint_id_, connection_id_, buffer_.data(),
                       buffer_.size());
  ExpectDecodedRequest(decode_result, received_request);
}

}  // namespace openscreen
