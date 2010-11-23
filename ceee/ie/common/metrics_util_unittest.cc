// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the CEEE metrics utilities.

#include <atlconv.h>

#include "ceee/ie/common/metrics_util.h"

#include <wtypes.h>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "base/string_util.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/mock_win32.h"
#include "ceee/testing/utils/test_utils.h"
#include "base/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::NotNull;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::Return;

class BrokerRpcClientMock : public BrokerRpcClient {
public:
  MOCK_METHOD2(SendUmaHistogramTimes, bool(BSTR, int));
  MOCK_METHOD5(SendUmaHistogramData, bool(BSTR, int, int, int, int));
};

class CeeeMetricsUtilTest : public testing::Test {
 protected:
  virtual void CreateAndDestroyTimer(BrokerRpcClient* rpc_client) {
    metrics_util::ScopedTimer timer("NonEmptyName", rpc_client);
  }
};

// Invalid broker or empty name initialization should CHECK(false).
TEST_F(CeeeMetricsUtilTest, ScopedTimerError) {
  testing::LogDisabler no_dcheck;
  // Since we can't test yet for NOTREACHED() or CHECK(false), do nothing.
  // TODO(hansl): when we can test for NOTREACHED, validate that passing invalid
  // values to the ScopedTimer will indeed fail.
}


// Test for successful uses of the ScopedTimer.
TEST_F(CeeeMetricsUtilTest, ScopedTimerSuccess) {
  testing::LogDisabler no_dcheck;

  // Test that timing is right. This should ultimately succeed.
  // We expect less than a couple milliseconds on this call.
  BrokerRpcClientMock broker_rpc;
  EXPECT_CALL(broker_rpc, SendUmaHistogramTimes(_, testing::Lt(10)));
  CreateAndDestroyTimer(&broker_rpc);
}

}  // namespace
