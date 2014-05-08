// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__arm__)  // nacl_irt_icache is supported only on ARM.

#include <errno.h>
#include <sys/mman.h>

#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NaClIrtIcacheTest, ClearCache) {
  int (*irt_clear_cache)(void*, size_t) =
      nacl::nonsfi::kIrtIcache.clear_cache;
  // Create code for a function to return 0x01.
  static const uint32_t code_template[] = {
    0xe3000001,  // movw r0, #0x1
    0xe12fff1e,  // bx lr
  };
  void* start =
      mmap(NULL, sizeof(code_template), PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  EXPECT_NE(MAP_FAILED, start);
  memcpy(start, code_template, sizeof(code_template));
  size_t size = sizeof(code_template);
  EXPECT_EQ(0, irt_clear_cache(start, size));
  typedef int (*TestFunc)(void);
  TestFunc func = reinterpret_cast<TestFunc>(start);
  EXPECT_EQ(0x1, func());
  // Modify the function to return 0x11.
  *reinterpret_cast<uint32_t*>(start) = 0xe3000011;  // movw r0, #0x11
  // In most cases 0x1 is still returned because the cached code is executed
  // although the CPU is not obliged to do so.
  // Uncomment the following line to see if the cached code is executed.
  // EXPECT_EQ(0x1, func());
  EXPECT_EQ(0, irt_clear_cache(start, size));
  // Now it is ensured that 0x11 is returned because I-cache was invalidated
  // and updated with the new code.
  EXPECT_EQ(0x11, func());
  EXPECT_EQ(0, munmap(start, sizeof(code_template)));
}

TEST(NaClIrtIcacheTest, ClearCacheInvalidArg) {
  int (*irt_clear_cache)(void*, size_t) =
      nacl::nonsfi::kIrtIcache.clear_cache;
  const size_t mem_size = 256;
  void* start =
      mmap(NULL, mem_size, PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  EXPECT_EQ(0, irt_clear_cache(start, mem_size));
  EXPECT_EQ(EINVAL, irt_clear_cache(start, 0));
  EXPECT_EQ(EINVAL, irt_clear_cache(NULL, mem_size));
  EXPECT_EQ(0, munmap(start, mem_size));
}

#endif
