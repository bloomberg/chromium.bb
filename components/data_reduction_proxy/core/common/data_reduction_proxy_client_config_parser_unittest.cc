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

const char kFutureTime[] = "31 Dec 2020 23:59:59.001";

}  // namespace

class DataReductionProxyClientConfigParserTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(base::Time::FromUTCString(kFutureTime, &future_time_));
  }

  const base::Time& GetFutureTime() {
    return future_time_;
  }

 private:
  base::Time future_time_;
};

TEST_F(DataReductionProxyClientConfigParserTest, TimetoTimestamp) {
  const struct {
    std::string test_name;
    base::Time time;
    int64 expected_seconds;
    int32 expected_nanos;
  } tests[] = {
      {
       "Epoch", base::Time::UnixEpoch(), 0, 0,
      },
      {
       "One day minus one second",
       base::Time::UnixEpoch() + base::TimeDelta::FromDays(1) -
           base::TimeDelta::FromMicroseconds(1),
       24 * 60 * 60 - 1,
       0,
      },
      {
       "Future time",
       GetFutureTime(),
       // 2020-12-31T23:59:59.001Z
       1609459199,
       0,
      },
  };

  for (const auto& test : tests) {
    Timestamp ts;
    config_parser::TimetoTimestamp(test.time, &ts);
    EXPECT_EQ(test.expected_seconds, ts.seconds()) << test.test_name;
    EXPECT_EQ(test.expected_nanos, ts.nanos()) << test.test_name;
  }
}

TEST_F(DataReductionProxyClientConfigParserTest, TimeDeltaToFromDuration) {
  const struct {
    std::string test_name;
    base::TimeDelta time_delta;
    int64 seconds;
    int32 nanos;
  } tests[] = {
      {
          "Second", base::TimeDelta::FromSeconds(1), 1, 0,
      },
      {
          "-1 Second", base::TimeDelta::FromSeconds(-1), -1, 0,
      },
  };

  for (const auto& test : tests) {
    Duration duration;
    config_parser::TimeDeltatoDuration(test.time_delta, &duration);
    EXPECT_EQ(test.seconds, duration.seconds()) << test.test_name;
    EXPECT_EQ(test.nanos, duration.nanos()) << test.test_name;
    duration.set_seconds(test.seconds);
    duration.set_nanos(test.nanos);
    EXPECT_EQ(test.time_delta, config_parser::DurationToTimeDelta(duration))
        << test.test_name;
  }
}

TEST_F(DataReductionProxyClientConfigParserTest, TimestampToTime) {
  const struct {
    std::string test_name;
    int64 timestamp_seconds;
    int32 timestamp_nanos;
    base::Time expected_time;
  } tests[] = {
      {
       "Epoch", 0, 0, base::Time::UnixEpoch(),
      },
      {
       "One day minus one second",
       24 * 60 * 60 - 1,
       base::Time::kNanosecondsPerSecond - 1,
       base::Time::UnixEpoch() + base::TimeDelta::FromDays(1) -
           base::TimeDelta::FromMicroseconds(1),
      },
      {
       "Future time",
       // 2020-12-31T23:59:59.001Z
       1609459199,
       1000000,
       GetFutureTime(),
      },
  };

  for (const auto& test : tests) {
    Timestamp ts;
    ts.set_seconds(test.timestamp_seconds);
    ts.set_nanos(test.timestamp_nanos);
    EXPECT_EQ(test.expected_time, config_parser::TimestampToTime(ts))
        << test.test_name;
  }
}

}  // namespace data_reduction_proxy
