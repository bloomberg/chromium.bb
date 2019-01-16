// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(ScopesLockManager, TestRangePopulation) {
  ScopeLockRange range{"a", "b"};
  EXPECT_EQ("a", range.begin);
  EXPECT_EQ("b", range.end);
}

}  // namespace
}  // namespace content
