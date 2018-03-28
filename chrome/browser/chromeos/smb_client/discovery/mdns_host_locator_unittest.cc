// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/mdns_host_locator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {

class MDnsHostLocatorTest : public testing::Test {
 public:
  MDnsHostLocatorTest() = default;
  ~MDnsHostLocatorTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MDnsHostLocatorTest);
};

TEST_F(MDnsHostLocatorTest, RemoveLocal) {
  EXPECT_EQ(RemoveLocal("QNAP"), "QNAP");
  EXPECT_EQ(RemoveLocal(".local-QNAP"), ".local-QNAP");
  EXPECT_EQ(RemoveLocal("QNAP.local"), "QNAP");
  EXPECT_EQ(RemoveLocal(".localQNAP.local"), ".localQNAP");
  EXPECT_EQ(RemoveLocal("QNAP.local.local"), "QNAP.local");
  EXPECT_EQ(RemoveLocal("QNAP.LOCAL"), "QNAP");
  EXPECT_EQ(RemoveLocal("QNAP.LoCaL"), "QNAP");
}

}  // namespace smb_client
}  // namespace chromeos
