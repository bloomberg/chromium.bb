// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncproto.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncProtoTest : public testing::Test {
};

TEST_F(SyncProtoTest, ProtocolVersionPresent) {
  ClientToServerMessage csm;
  EXPECT_TRUE(csm.has_protocol_version());
}

}  // namespace browser_sync
