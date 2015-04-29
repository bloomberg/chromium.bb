// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/memory/memory_wedge.h"

#include "base/macros.h"
#include "ios/chrome/browser/memory/memory_metrics.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_wedge {

namespace {

// The number of bytes in a megabyte.
const uint64 kNumBytesInMB = 1024 * 1024;

// Note: in the following tests, only memory_util::GetInternalVMBytes,
// and memory_util::GetRealMemoryUsedInBytes are used to check the state of the
// memory before and after an action.
// memory_util::GetFreePhysicalBytes and memory_util::GetDirtyVMBytes are not
// used because external events can change these values, making them not
// reliable.

// Performs a snapshot of the memory when constructed. Deviation from the
// initial values can be verified with VerifyDeviation. The comparison is
// on the integer part of the values in MB.
class MemoryChecker {
 public:
  MemoryChecker() {
    internal_vm_ = memory_util::GetInternalVMBytes() / kNumBytesInMB;
    real_memory_used_ = memory_util::GetRealMemoryUsedInBytes() / kNumBytesInMB;
  }

  // Verifies that the memory metrics deviated only by |deviation| in MB.
  void VerifyDeviation(uint64 deviation) const {
    EXPECT_NEAR(memory_util::GetInternalVMBytes() / kNumBytesInMB,
                deviation + internal_vm_, 1);
    EXPECT_NEAR(memory_util::GetRealMemoryUsedInBytes() / kNumBytesInMB,
                deviation + real_memory_used_, 1);
  }

 private:
  uint64 internal_vm_;
  uint64 real_memory_used_;

  DISALLOW_COPY_AND_ASSIGN(MemoryChecker);
};

class MemoryWedgeTest : public testing::TestWithParam<size_t> {};

}  // namespace

// Checks that the wedge size passed to AddWedge is indeed added to the global
// footprint of the app.
TEST_P(MemoryWedgeTest, WedgeSize) {
  const MemoryChecker memory_checker;
  size_t wedge_size = GetParam();

  AddWedge(wedge_size);

  memory_checker.VerifyDeviation(wedge_size);
  if (wedge_size > 0)
    RemoveWedgeForTesting();
}

INSTANTIATE_TEST_CASE_P(
    /* No InstantiationName */,
    MemoryWedgeTest,
    testing::Values(0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100));

}  // namespace memory_wedge
