// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/winheap_dump_provider_win.h"

#include <windows.h>

#include "base/trace_event/memory_dump_session_state.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

class WinHeapDumpProviderTest : public testing::Test {
 public:
  bool GetHeapInformation(WinHeapDumpProvider* provider,
                          WinHeapInfo* heap_info,
                          const std::set<void*>& block_to_skip) {
    return provider->GetHeapInformation(heap_info, block_to_skip);
  }
};

TEST_F(WinHeapDumpProviderTest, DumpInto) {
  ProcessMemoryDump pmd(make_scoped_refptr(new MemoryDumpSessionState()));

  WinHeapDumpProvider* winheap_dump_provider =
      WinHeapDumpProvider::GetInstance();
  ASSERT_NE(reinterpret_cast<WinHeapDumpProvider*>(nullptr),
            winheap_dump_provider);

  ASSERT_TRUE(winheap_dump_provider->DumpInto(&pmd));
}

TEST_F(WinHeapDumpProviderTest, GetHeapInformation) {
  ProcessMemoryDump pmd(make_scoped_refptr(new MemoryDumpSessionState()));

  WinHeapDumpProvider* winheap_dump_provider =
      WinHeapDumpProvider::GetInstance();
  ASSERT_NE(reinterpret_cast<WinHeapDumpProvider*>(nullptr),
            winheap_dump_provider);

  HANDLE heap = ::HeapCreate(NULL, 0, 0);
  ASSERT_NE(nullptr, heap);

  const size_t kAllocSize = 42;
  void* alloc = ::HeapAlloc(heap, 0, kAllocSize);
  ASSERT_NE(nullptr, alloc);

  WinHeapInfo heap_info = {0};
  heap_info.heap_id = heap;
  std::set<void*> block_to_skip;

  // Put the allocation into the skip list and make sure that the provider
  // ignores it.
  block_to_skip.insert(alloc);
  ASSERT_TRUE(
      GetHeapInformation(winheap_dump_provider, &heap_info, block_to_skip));
  EXPECT_EQ(0U, heap_info.block_count);
  EXPECT_EQ(0U, heap_info.allocated_size);
  // We can't check the committed size here, as it can depend on the version of
  // kernel32.dll.

  // Remove the allocation from the skip list and check if it's analysed
  // properlyly.
  block_to_skip.erase(alloc);
  ASSERT_TRUE(
      GetHeapInformation(winheap_dump_provider, &heap_info, block_to_skip));
  EXPECT_EQ(1, heap_info.block_count);
  EXPECT_EQ(kAllocSize, heap_info.allocated_size);
  EXPECT_LT(kAllocSize, heap_info.committed_size);

  EXPECT_TRUE(::HeapFree(heap, 0, alloc));
  EXPECT_EQ(TRUE, ::HeapDestroy(heap));
}

}  // namespace trace_event
}  // namespace base
