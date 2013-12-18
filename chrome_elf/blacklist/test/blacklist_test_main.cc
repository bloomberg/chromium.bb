// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "chrome_elf/blacklist/test/blacklist_test_main_dll.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  base::AtExitManager at_exit_manager;

  InitBlacklistTestDll();

  RUN_ALL_TESTS();
}
