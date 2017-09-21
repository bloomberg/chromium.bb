// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/freed_object_tracker.h"

#include <memory>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/features.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

class FreedObjectTrackerTest : public testing::Test {
 public:
  FreedObjectTrackerTest() {}
  ~FreedObjectTrackerTest() override {}

  // A 24-byte class for testing.
  struct Size24 {
    char bytes[24];
  };

  void SetUp() override {
#if defined(OS_MACOSX) && BUILDFLAG(USE_ALLOCATOR_SHIM)
    allocator::InitializeAllocatorShim();
#endif
    FreedObjectTracker::GetInstance()->Enable();
  }

  void TearDown() override {
    FreedObjectTracker::GetInstance()->DisableForTesting();
  }
};

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#define MAYBE_Get Get
#else
#define MAYBE_Get DISABLED_Get
#endif
TEST_F(FreedObjectTrackerTest, MAYBE_Get) {
  FreedObjectTracker* tracker = FreedObjectTracker::GetInstance();
  AllocationRegister::Allocation allocation;

  std::unique_ptr<Size24> obj = std::make_unique<Size24>();
  Size24* objptr = obj.get();
  ASSERT_FALSE(tracker->Get(objptr, &allocation));

  obj.reset();
  ASSERT_TRUE(tracker->Get(objptr, &allocation));
  EXPECT_EQ(objptr, allocation.address);
  EXPECT_LT(0U, allocation.context.backtrace.frame_count);
}

}  // namespace trace_event
}  // namespace base
