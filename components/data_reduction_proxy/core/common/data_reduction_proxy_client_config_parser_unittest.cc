// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"

#include <string>

#include "base/time/time.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

const char kValidPreamble[] =
    "{ \"sessionKey\": \"foobar\", "
    "\"expireTime\": \"2100-12-31T23:59:59.999999Z\", "
    "\"proxyConfig\": { \"httpProxyServers\": [";
const char kValidPostamble[] = "] } }";

}  // namespace

TEST(ClientConfigParserTest, TimeToISO8601) {
  const struct {
    base::Time time;
    std::string expected;
  } tests[] = {
      {
          base::Time(),
          "1601-01-01T00:00:00.000Z",
      },
      {
          base::Time::UnixEpoch(),
          "1970-01-01T00:00:00.000Z",
      },
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expected, config_parser::TimeToISO8601(test.time));
  }
}

TEST(ClientConfigParserTest, ISO8601ToTimestamp) {
  const struct {
    std::string time_string;
    int64 epoch_seconds;
  } tests[] = {
      {
          "1970-01-01T00:00:00.000Z",
          0,
      },
      {
          "1970-01-01T00:00:00.999Z",
          0,
      },
      {
          "1950-01-01T00:00:00.000Z",
          -631152000,
      },
      {
          "1950-01-01T00:00:00.500Z",
          -631151999,  // Rounding of negative fractional values causes this.
      },
  };

  for (const auto& test : tests) {
    Timestamp timestamp;
    EXPECT_TRUE(
        config_parser::ISO8601ToTimestamp(test.time_string, &timestamp));
    EXPECT_EQ(test.epoch_seconds, timestamp.seconds());
    EXPECT_EQ(0, timestamp.nanos());
  }
}

TEST(ClientConfigParserTest, ISO8601ToTimestampTestFailures) {
  const std::string inputs[] = {
      "",
      "Not a time",
      "1234",
      "2099-43-12",
      "2099-11-52",
  };

  for (const auto& input : inputs) {
    Timestamp timestamp;
    EXPECT_FALSE(config_parser::ISO8601ToTimestamp(input, &timestamp));
  }
}

TEST(ClientConfigParserTest, TimestampToTime) {
  base::Time::Exploded future = {
      2100,
      12,
      5,
      31,
      23,
      59,
      59,
      0,
  };
  const struct {
    int64 timestamp_seconds;
    int32 timestamp_nanos;
    base::Time expected_time;
  } tests[] = {
      {
        0,
        0,
        base::Time::UnixEpoch(),
      },
      {
        24 * 60 * 60 - 1,
        base::Time::kNanosecondsPerSecond - 1,
        base::Time::UnixEpoch() + base::TimeDelta::FromDays(1) -
            base::TimeDelta::FromMicroseconds(1),
      },
      {
          // 2100-12-31T23:59:59.000Z
          4133980799,
          0,
          base::Time::FromUTCExploded(future),
      },
  };

  for (const auto& test : tests) {
    Timestamp ts;
    ts.set_seconds(test.timestamp_seconds);
    ts.set_nanos(test.timestamp_nanos);
    EXPECT_EQ(test.expected_time, config_parser::TimestampToTime(ts));
  }
}

TEST(ClientConfigParserTest, SingleServer) {
  const std::string inputs[] = {
    "{ \"scheme\": \"HTTP\", \"host\": \"foo.com\", \"port\": 80 }",
    "{ \"scheme\": \"HTTPS\", \"host\": \"foo.com\", \"port\": 443 }",
    "{ \"scheme\": \"QUIC\", \"host\": \"foo.com\", \"port\": 443 }",
  };

  for (const auto& scheme_fragment : inputs) {
    std::string input = kValidPreamble + scheme_fragment + kValidPostamble;
    ClientConfig config;
    EXPECT_TRUE(config_parser::ParseClientConfig(input, &config));
    EXPECT_EQ(1, config.proxy_config().http_proxy_servers_size());
  }
}

TEST(ClientConfigParserTest, MultipleServers) {
  std::string input =
      "{ \"scheme\": \"HTTP\", ""\"host\": \"foo.com\", \"port\": 80 }, "
      "{ \"scheme\": \"HTTPS\", \"host\": \"foo.com\", \"port\": 443 }, "
      "{ \"scheme\": \"QUIC\", \"host\": \"foo.com\", \"port\": 443 }";
  ClientConfig config;
  EXPECT_TRUE(config_parser::ParseClientConfig(
      kValidPreamble + input + kValidPostamble, &config));
  EXPECT_EQ(3, config.proxy_config().http_proxy_servers_size());
}

TEST(ClientConfigParserTest, FailureCases) {
  const std::string inputs[] = {
    "",
    "invalid json",
    // No sessionKey
    "{ \"expireTime\": \"2100-12-31T23:59:59.999999Z\","
      "\"proxyConfig\": { \"httpProxyServers\": [ ] } }",
    // No expireTime
    "{ \"sessionKey\": \"foobar\","
      "\"proxyConfig\": { \"httpProxyServers\": [ ] } }",
    // No proxyConfig
    "{ \"sessionKey\": \"foobar\", "
      "\"expireTime\": \"2100-12-31T23:59:59.999999Z\" }",
    // No proxyConfig.httpProxyServers
    "{ \"sessionKey\": \"foobar\", "
      "\"expireTime\": \"2100-12-31T23:59:59.999999Z\", \"proxyConfig\": { } }",
    // Invalid sessionKey
    "{ \"sessionKey\": 12345, "
      "\"expireTime\": \"2100-12-31T23:59:59.999999Z\","
      " \"proxyConfig\": { \"httpProxyServers\": [ ] } }",
    // Invalid sessionKey
    "{ \"sessionKey\": { }, "
      "\"expireTime\": \"2100-12-31T23:59:59.999999Z\","
      " \"proxyConfig\": { \"httpProxyServers\": [ ] } }",
    // Invalid sessionKey
    "{ \"sessionKey\": [ ], "
      "\"expireTime\": \"2100-12-31T23:59:59.999999Z\","
      " \"proxyConfig\": { \"httpProxyServers\": [ ] } }",
    // Invalid expireTime
    "{ \"sessionKey\": \"foobar\", "
      "\"expireTime\": \"abcd\","
      " \"proxyConfig\": { \"httpProxyServers\": [ ] } }",
    // Invalid expireTime
    "{ \"sessionKey\": \"foobar\", "
      "\"expireTime\": [ ],"
      " \"proxyConfig\": { \"httpProxyServers\": [ ] } }",
    // Invalid expireTime
    "{ \"sessionKey\": \"foobar\", "
      "\"expireTime\": { },"
      " \"proxyConfig\": { \"httpProxyServers\": [ ] } }",
  };

  for (const auto& input : inputs) {
    ClientConfig config;
    EXPECT_FALSE(config_parser::ParseClientConfig(input, &config));
  }
}

TEST(ClientConfigParserTest, EmptyServerLists) {
  const std::string inputs[] = {
    "",
    "{ }",
    "{ \"scheme\": \"foo\", \"host\": \"foo.com\", \"port\": 80 }",
    "{ \"scheme\": \"HTTP\", \"port\": 80 }",
    "{ \"scheme\": \"HTTP\", \"host\": \"foo.com\" }",
    "{ \"scheme\": \"HTTP\", \"host\": \"foo.com\", \"port\": \"bar\" }",
    "{ \"scheme\": \"HTTP\", \"host\": 12345, \"port\": 80 }",
  };

  for (const auto& scheme_fragment : inputs) {
    std::string input = kValidPreamble + scheme_fragment + kValidPostamble;
    ClientConfig config;
    EXPECT_TRUE(config_parser::ParseClientConfig(input, &config));
    EXPECT_EQ(0, config.proxy_config().http_proxy_servers_size());
  }
}

}  // namespace data_reduction_proxy
