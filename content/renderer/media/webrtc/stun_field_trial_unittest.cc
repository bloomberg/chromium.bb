// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/stun_field_trial.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/base/socketaddress.h"

namespace content {

TEST(StunProbeTrial, VerifyParameterParsing) {
  std::vector<rtc::SocketAddress> servers;
  int requests_per_ip;
  int interval_ms;
  int shared_socket_mode;
  std::string params;

  params = "20/500/1/server:3478/server2:3478";
  EXPECT_TRUE(ParseStunProbeParameters(params, &requests_per_ip, &interval_ms,
                                       &shared_socket_mode, &servers));
  EXPECT_EQ(requests_per_ip, 20);
  EXPECT_EQ(interval_ms, 100);
  EXPECT_EQ(shared_socket_mode, 1);
  EXPECT_EQ(servers.size(), 2u);
  EXPECT_EQ(servers[0], rtc::SocketAddress("server", 3478));
  EXPECT_EQ(servers[1], rtc::SocketAddress("server2", 3478));
  servers.clear();

  params = "///server:3478";
  EXPECT_TRUE(ParseStunProbeParameters(params, &requests_per_ip, &interval_ms,
                                       &shared_socket_mode, &servers));
  EXPECT_EQ(requests_per_ip, 10);
  EXPECT_EQ(servers.size(), 1u);
  EXPECT_EQ(servers[0], rtc::SocketAddress("server", 3478));
  servers.clear();

  params = "////";
  EXPECT_FALSE(ParseStunProbeParameters(params, &requests_per_ip, &interval_ms,
                                        &shared_socket_mode, &servers));
}

}  // namespace content
