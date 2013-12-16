// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_config.h"

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"
#include "net/quic/quic_time.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {
namespace {

class QuicConfigTest : public ::testing::Test {
 protected:
  QuicConfigTest() {
    config_.SetDefaults();
    config_.set_initial_round_trip_time_us(kMaxInitialRoundTripTimeUs, 0);
  }

  QuicConfig config_;
};

TEST_F(QuicConfigTest, ToHandshakeMessage) {
  FLAGS_enable_quic_pacing = false;
  config_.SetDefaults();
  config_.set_idle_connection_state_lifetime(QuicTime::Delta::FromSeconds(5),
                                             QuicTime::Delta::FromSeconds(2));
  config_.set_max_streams_per_connection(4, 2);
  CryptoHandshakeMessage msg;
  config_.ToHandshakeMessage(&msg);

  uint32 value;
  QuicErrorCode error = msg.GetUint32(kICSL, &value);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_EQ(5u, value);

  error = msg.GetUint32(kMSPC, &value);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_EQ(4u, value);

  const QuicTag* out;
  size_t out_len;
  error = msg.GetTaglist(kCGST, &out, &out_len);
  EXPECT_EQ(1u, out_len);
  EXPECT_EQ(kQBIC, *out);
}

TEST_F(QuicConfigTest, ToHandshakeMessageWithPacing) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_pacing, true);

  config_.SetDefaults();
  CryptoHandshakeMessage msg;
  config_.ToHandshakeMessage(&msg);

  const QuicTag* out;
  size_t out_len;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetTaglist(kCGST, &out, &out_len));
  EXPECT_EQ(2u, out_len);
  EXPECT_EQ(kPACE, out[0]);
  EXPECT_EQ(kQBIC, out[1]);
}

TEST_F(QuicConfigTest, ProcessClientHello) {
  QuicConfig client_config;
  QuicTagVector cgst;
  cgst.push_back(kINAR);
  cgst.push_back(kQBIC);
  client_config.set_congestion_control(cgst, kQBIC);
  client_config.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(2 * kDefaultTimeoutSecs),
      QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs));
  client_config.set_max_streams_per_connection(
      2 * kDefaultMaxStreamsPerConnection, kDefaultMaxStreamsPerConnection);
  client_config.set_initial_round_trip_time_us(
      10 * base::Time::kMicrosecondsPerMillisecond,
      10 * base::Time::kMicrosecondsPerMillisecond);

  CryptoHandshakeMessage msg;
  client_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error = config_.ProcessClientHello(msg, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_TRUE(config_.negotiated());
  EXPECT_EQ(kQBIC, config_.congestion_control());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs),
            config_.idle_connection_state_lifetime());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            config_.max_streams_per_connection());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(0), config_.keepalive_timeout());
  EXPECT_EQ(10 * base::Time::kMicrosecondsPerMillisecond,
            config_.initial_round_trip_time_us());
}

TEST_F(QuicConfigTest, ProcessServerHello) {
  QuicConfig server_config;
  QuicTagVector cgst;
  cgst.push_back(kQBIC);
  server_config.set_congestion_control(cgst, kQBIC);
  server_config.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs / 2),
      QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs / 2));
  server_config.set_max_streams_per_connection(
      kDefaultMaxStreamsPerConnection / 2,
      kDefaultMaxStreamsPerConnection / 2);
  server_config.set_server_initial_congestion_window(kDefaultInitialWindow / 2,
                                                     kDefaultInitialWindow / 2);
  server_config.set_initial_round_trip_time_us(
      10 * base::Time::kMicrosecondsPerMillisecond,
      10 * base::Time::kMicrosecondsPerMillisecond);

  CryptoHandshakeMessage msg;
  server_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error = config_.ProcessServerHello(msg, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);
  EXPECT_TRUE(config_.negotiated());
  EXPECT_EQ(kQBIC, config_.congestion_control());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs / 2),
            config_.idle_connection_state_lifetime());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection / 2,
            config_.max_streams_per_connection());
  EXPECT_EQ(kDefaultInitialWindow / 2,
            config_.server_initial_congestion_window());
  EXPECT_EQ(QuicTime::Delta::FromSeconds(0), config_.keepalive_timeout());
  EXPECT_EQ(10 * base::Time::kMicrosecondsPerMillisecond,
            config_.initial_round_trip_time_us());
}

TEST_F(QuicConfigTest, MissingValueInCHLO) {
  CryptoHandshakeMessage msg;
  msg.SetValue(kICSL, 1);
  msg.SetVector(kCGST, QuicTagVector(1, kQBIC));
  // Missing kMSPC. KATO is optional.
  string error_details;
  const QuicErrorCode error = config_.ProcessClientHello(msg, &error_details);
  EXPECT_EQ(QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND, error);
}

TEST_F(QuicConfigTest, MissingValueInSHLO) {
  CryptoHandshakeMessage msg;
  msg.SetValue(kICSL, 1);
  msg.SetValue(kMSPC, 3);
  // Missing CGST. KATO is optional.
  string error_details;
  const QuicErrorCode error = config_.ProcessServerHello(msg, &error_details);
  EXPECT_EQ(QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND, error);
}

TEST_F(QuicConfigTest, OutOfBoundSHLO) {
  QuicConfig server_config;
  server_config.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(2 * kDefaultTimeoutSecs),
      QuicTime::Delta::FromSeconds(2 * kDefaultTimeoutSecs));

  CryptoHandshakeMessage msg;
  server_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error = config_.ProcessServerHello(msg, &error_details);
  EXPECT_EQ(QUIC_INVALID_NEGOTIATED_VALUE, error);
}

TEST_F(QuicConfigTest, MultipleNegotiatedValuesInVectorTag) {
  QuicConfig server_config;
  QuicTagVector cgst;
  cgst.push_back(kQBIC);
  cgst.push_back(kINAR);
  server_config.set_congestion_control(cgst, kQBIC);

  CryptoHandshakeMessage msg;
  server_config.ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error = config_.ProcessServerHello(msg, &error_details);
  EXPECT_EQ(QUIC_INVALID_NEGOTIATED_VALUE, error);
}

TEST_F(QuicConfigTest, NoOverLapInCGST) {
  QuicConfig server_config;
  server_config.SetDefaults();
  QuicTagVector cgst;
  cgst.push_back(kINAR);
  server_config.set_congestion_control(cgst, kINAR);

  CryptoHandshakeMessage msg;
  string error_details;
  server_config.ToHandshakeMessage(&msg);
  const QuicErrorCode error = config_.ProcessClientHello(msg, &error_details);
  LOG(INFO) << QuicUtils::ErrorToString(error);
  EXPECT_EQ(QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP, error);
}

}  // namespace
}  // namespace test
}  // namespace net
