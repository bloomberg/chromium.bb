// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/web_socket_encoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class WebSocketEncoderTest : public testing::Test {
 public:
  WebSocketEncoderTest() {}

  void SetUp() override {
    std::string response_extensions;
    server_.reset(WebSocketEncoder::CreateServer("", &response_extensions));
    EXPECT_EQ(std::string(), response_extensions);
    client_.reset(WebSocketEncoder::CreateClient(""));
  }

 protected:
  scoped_ptr<WebSocketEncoder> server_;
  scoped_ptr<WebSocketEncoder> client_;
};

class WebSocketEncoderCompressionTest : public WebSocketEncoderTest {
 public:
  WebSocketEncoderCompressionTest() : WebSocketEncoderTest() {}

  void SetUp() override {
    std::string response_extensions;
    server_.reset(WebSocketEncoder::CreateServer(
        "permessage-deflate; client_max_window_bits", &response_extensions));
    EXPECT_EQ(
        "permessage-deflate; server_max_window_bits=15; "
        "client_max_window_bits=15",
        response_extensions);
    client_.reset(WebSocketEncoder::CreateClient(response_extensions));
  }
};

TEST_F(WebSocketEncoderTest, ClientToServer) {
  std::string frame("ClientToServer");
  int mask = 123456;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  client_->EncodeFrame(frame, mask, &encoded);
  EXPECT_EQ(WebSocket::FRAME_OK,
            server_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  std::string partial = encoded.substr(0, encoded.length() - 2);
  EXPECT_EQ(WebSocket::FRAME_INCOMPLETE,
            server_->DecodeFrame(partial, &bytes_consumed, &decoded));

  std::string extra = encoded + "more stuff";
  EXPECT_EQ(WebSocket::FRAME_OK,
            server_->DecodeFrame(extra, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  EXPECT_EQ(
      WebSocket::FRAME_ERROR,
      server_->DecodeFrame(std::string("abcde"), &bytes_consumed, &decoded));
}

TEST_F(WebSocketEncoderTest, ServerToClient) {
  std::string frame("ServerToClient");
  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeFrame(frame, mask, &encoded);
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  std::string partial = encoded.substr(0, encoded.length() - 2);
  EXPECT_EQ(WebSocket::FRAME_INCOMPLETE,
            client_->DecodeFrame(partial, &bytes_consumed, &decoded));

  std::string extra = encoded + "more stuff";
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(extra, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  EXPECT_EQ(
      WebSocket::FRAME_ERROR,
      client_->DecodeFrame(std::string("abcde"), &bytes_consumed, &decoded));
}

TEST_F(WebSocketEncoderCompressionTest, ClientToServer) {
  std::string frame("CompressionCompressionCompressionCompression");
  int mask = 654321;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  client_->EncodeFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocket::FRAME_OK,
            server_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, ServerToClient) {
  std::string frame("CompressionCompressionCompressionCompression");
  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, LongFrame) {
  int length = 1000000;
  std::string temp;
  temp.reserve(length);
  for (int i = 0; i < length; ++i)
    temp += (char)('a' + (i % 26));

  std::string frame;
  frame.reserve(length);
  for (int i = 0; i < length; ++i) {
    int64 j = i;
    frame += temp.data()[(j * j) % length];
  }

  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

}  // namespace net
