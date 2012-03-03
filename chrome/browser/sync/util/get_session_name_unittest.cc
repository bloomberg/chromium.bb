// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/sync/util/get_session_name.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

typedef testing::Test SyncGetSessionNameTaskTest;

#if defined(OS_WIN)
// The test is somewhat silly, and just verifies that we return a computer name.
TEST_F(SyncGetSessionNameTaskTest, GetComputerName) {
  std::string computer_name = internal::GetComputerName();
  EXPECT_TRUE(!computer_name.empty());
}
#endif

#if defined(OS_MACOSX)
// The test is somewhat silly, and just verifies that we return a hardware
// model name.
TEST_F(SyncGetSessionNameTaskTest, GetHardwareModelName) {
  std::string hardware_model = internal::GetHardwareModelName();
  EXPECT_TRUE(!hardware_model.empty());
}
#endif

}
