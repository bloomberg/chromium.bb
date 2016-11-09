// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

base::AtExitManager* g_at_exit_ = nullptr;

int main(int argc, char* argv[]) {
  g_at_exit_ = new base::AtExitManager;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
