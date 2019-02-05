// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(ScopesLockManager, TestRangePopulation) {
  ScopeLockRange range = {"1", "2"};
  EXPECT_EQ("1", range.begin);
  EXPECT_EQ("2", range.end);
  EXPECT_TRUE(range.IsValid());
}

TEST(ScopesLockManager, TestInvalidRange) {
  ScopeLockRange range = {"2", "1"};
  EXPECT_FALSE(range.IsValid());
  range = {"2", "2"};
  EXPECT_FALSE(range.IsValid());
}

TEST(ScopesLockManager, TestRangeDifferences) {
  ScopeLockRange range_db1;
  ScopeLockRange range_db2;
  ScopeLockRange range_db1_os1;
  ScopeLockRange range_db1_os2;
  for (int64_t i = 0; i < 512; ++i) {
    range_db1 = GetDatabaseLockRange(i);
    range_db2 = GetDatabaseLockRange(i + 1);
    range_db1_os1 = GetObjectStoreLockRange(i, i);
    range_db1_os2 = GetObjectStoreLockRange(i, i + 1);
    EXPECT_TRUE(range_db1.IsValid() && range_db2.IsValid() &&
                range_db1_os1.IsValid() && range_db1_os2.IsValid());
    EXPECT_LT(range_db1, range_db2);
    EXPECT_LT(range_db1, range_db1_os1);
    EXPECT_LT(range_db1, range_db1_os2);
    EXPECT_LT(range_db1_os1, range_db1_os2);
    EXPECT_LT(range_db1_os1, range_db2);
    EXPECT_LT(range_db1_os2, range_db2);
  }
}

}  // namespace
}  // namespace content
