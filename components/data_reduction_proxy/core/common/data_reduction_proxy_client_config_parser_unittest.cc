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

}  // namespace data_reduction_proxy
