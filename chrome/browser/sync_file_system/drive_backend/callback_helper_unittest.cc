// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

void SimpleCallback(bool* called, int) {
  ASSERT_TRUE(called);
  EXPECT_FALSE(*called);
  *called = true;
}

void CallbackWithPassed(bool* called, scoped_ptr<int>) {
  ASSERT_TRUE(called);
  EXPECT_FALSE(*called);
  *called = true;
}

}  // namespace

TEST(DriveBackendCallbackHelperTest, CreateRelayedCallbackTest) {
  base::MessageLoop message_loop;

  bool called = false;
  CreateRelayedCallback(base::Bind(&SimpleCallback, &called)).Run(0);
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  called = false;
  CreateRelayedCallback(
      base::Bind(&CallbackWithPassed, &called))
      .Run(scoped_ptr<int>(new int));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

}  // namespace drive_backend
}  // namespace sync_file_system
