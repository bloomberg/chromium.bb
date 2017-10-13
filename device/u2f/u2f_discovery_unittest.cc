// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_discovery.h"

#include "device/u2f/mock_u2f_discovery.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(U2fDiscoveryTest, SetAndResetDelegate) {
  MockU2fDiscovery discovery;
  EXPECT_FALSE(discovery.delegate());

  MockU2fDiscovery::MockDelegate delegate;
  discovery.SetDelegate(delegate.GetWeakPtr());
  EXPECT_EQ(&delegate, discovery.delegate());

  discovery.SetDelegate(nullptr);
  EXPECT_FALSE(discovery.delegate());
}

}  // namespace device
