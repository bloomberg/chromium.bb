// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tracked_objects {

#if defined(COMPILER_MSVC) || (defined(COMPILER_GCC) && !defined(OS_NACL))
TEST(LocationTest, TestProgramCounter) {
  Location loc1 = FROM_HERE;
  const void* pc1 = GetProgramCounter();
  const void* pc2 = GetProgramCounter();
  Location loc2 = FROM_HERE;

  EXPECT_LT(loc1.program_counter(), pc1);
  EXPECT_LT(pc1, pc2);
  EXPECT_LT(pc2, loc2.program_counter());
}
#endif // defined(COMPILER_MSVC) || (defined(COMPILER_GCC) && !defined(OS_NACL))

}  // namespace base
