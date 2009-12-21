// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mach_broker_mac.h"

#include "testing/gtest/include/gtest/gtest.h"

class MachBrokerTest : public testing::Test {
 public:
  MachBroker broker_;
};

TEST_F(MachBrokerTest, Setter) {
  broker_.RegisterPid(1u, MachBroker::MachInfo().SetTask(2u));
  EXPECT_EQ(2u, broker_.TaskForPid(1));
  EXPECT_EQ(0u, broker_.TaskForPid(2));
}

TEST_F(MachBrokerTest, Invalidate) {
  broker_.RegisterPid(1u, MachBroker::MachInfo().SetTask(2));
  broker_.Invalidate(1u);
  EXPECT_EQ(0u, broker_.TaskForPid(1));
}
