// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::nothrow;

namespace {

// Check that we can not allocate a memory range that cannot be indexed
// via an int. This is used to mitigate vulnerabilities in libraries that use
// int instead of size_t.
// See crbug.com/169327.

// - NO_TCMALLOC because we only patched tcmalloc
// - ADDRESS_SANITIZER because it has its own memory allocator
// - IOS does not seem to honor nothrow in new properly
// - OS_MACOSX does not use tcmalloc
#if !defined(NO_TCMALLOC) && !defined(ADDRESS_SANITIZER) && \
    !defined(OS_IOS) && !defined(OS_MACOSX)
  #define ALLOC_TEST(function) function
#else
  #define ALLOC_TEST(function) DISABLED_##function
#endif

// TODO(jln): switch to std::numeric_limits<int>::max() when we switch to
// C++11.
const size_t kTooBigAllocSize = INT_MAX;

// Detect runtime TCMalloc bypasses.
bool IsTcMallocBypassed() {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // This should detect a TCMalloc bypass from Valgrind.
  char* g_slice = getenv("G_SLICE");
  if (g_slice && !strcmp(g_slice, "always-malloc"))
    return true;
#endif
  return false;
}

// Fake test that allow to know the state of TCMalloc by looking at bots.
TEST(SecurityTest, ALLOC_TEST(IsTCMallocDynamicallyBypassed)) {
  printf("Malloc is dynamically bypassed: %s\n",
         IsTcMallocBypassed() ? "yes." : "no.");
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsMalloc)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<char, base::FreeDeleter>
        ptr(static_cast<char*>(malloc(kTooBigAllocSize)));
    ASSERT_TRUE(ptr == NULL);
  }
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsCalloc)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<char, base::FreeDeleter>
        ptr(static_cast<char*>(calloc(kTooBigAllocSize, 1)));
    ASSERT_TRUE(ptr == NULL);
  }
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsRealloc)) {
  if (!IsTcMallocBypassed()) {
    char* orig_ptr = static_cast<char*>(malloc(1));
    ASSERT_TRUE(orig_ptr != NULL);
    scoped_ptr<char, base::FreeDeleter>
        ptr(static_cast<char*>(realloc(orig_ptr, kTooBigAllocSize)));
    ASSERT_TRUE(ptr == NULL);
    // If realloc() did not succeed, we need to free orig_ptr.
    free(orig_ptr);
  }
}

typedef struct {
  char large_array[kTooBigAllocSize];
} VeryLargeStruct;

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsNew)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<VeryLargeStruct> ptr(new (nothrow) VeryLargeStruct);
    ASSERT_TRUE(ptr == NULL);
  }
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsNewArray)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<char[]> ptr(new (nothrow) char[kTooBigAllocSize]);
    ASSERT_TRUE(ptr == NULL);
  }
}

}  // namespace
