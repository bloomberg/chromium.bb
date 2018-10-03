// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/wait_for_network_callback_helper.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

// A test fixture to test WaitForNetworkCallbackHelper.
class WaitForNetworkCallbackHelperTest : public testing::Test {
 public:
  void CallbackFunction() { num_callbacks_invoked_++; }

 protected:
  WaitForNetworkCallbackHelperTest()
      : num_callbacks_invoked_(0),
        callback_helper_(network::TestNetworkConnectionTracker::GetInstance()) {
  }

  int num_callbacks_invoked_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  WaitForNetworkCallbackHelper callback_helper_;
};

TEST_F(WaitForNetworkCallbackHelperTest, CallbackInvokedImmediately) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  callback_helper_.HandleCallback(
      base::Bind(&WaitForNetworkCallbackHelperTest::CallbackFunction,
                 base::Unretained(this)));
  EXPECT_EQ(1, num_callbacks_invoked_);
}

TEST_F(WaitForNetworkCallbackHelperTest, CallbackInvokedLater) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  callback_helper_.HandleCallback(
      base::Bind(&WaitForNetworkCallbackHelperTest::CallbackFunction,
                 base::Unretained(this)));
  callback_helper_.HandleCallback(
      base::Bind(&WaitForNetworkCallbackHelperTest::CallbackFunction,
                 base::Unretained(this)));
  EXPECT_EQ(0, num_callbacks_invoked_);

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(2, num_callbacks_invoked_);
}
