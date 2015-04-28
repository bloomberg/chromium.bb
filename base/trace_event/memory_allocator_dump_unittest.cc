// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"

#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_session_state.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

namespace {

class FakeMemoryAllocatorDumpProvider : public MemoryDumpProvider {
 public:
  bool OnMemoryDump(ProcessMemoryDump* pmd) override {
    MemoryAllocatorDump* root_heap = pmd->CreateAllocatorDump(
        "foobar_allocator", MemoryAllocatorDump::kRootHeap);
    root_heap->set_physical_size_in_bytes(4096);
    root_heap->set_allocated_objects_count(42);
    root_heap->set_allocated_objects_size_in_bytes(1000);

    MemoryAllocatorDump* sub_heap =
        pmd->CreateAllocatorDump("foobar_allocator", "sub_heap");
    sub_heap->set_physical_size_in_bytes(1);
    sub_heap->set_allocated_objects_count(2);
    sub_heap->set_allocated_objects_size_in_bytes(3);

    pmd->CreateAllocatorDump("foobar_allocator", "sub_heap/empty");
    // Leave the rest of sub heap deliberately uninitialized, to check that
    // CreateAllocatorDump returns a properly zero-initialized object.

    return true;
  }
};
}  // namespace

TEST(MemoryAllocatorDumpTest, DumpIntoProcessMemoryDump) {
  FakeMemoryAllocatorDumpProvider fmadp;
  ProcessMemoryDump pmd(make_scoped_refptr(new MemoryDumpSessionState()));

  fmadp.OnMemoryDump(&pmd);

  ASSERT_EQ(3u, pmd.allocator_dumps().size());

  const MemoryAllocatorDump* root_heap =
      pmd.GetAllocatorDump("foobar_allocator", MemoryAllocatorDump::kRootHeap);
  ASSERT_NE(nullptr, root_heap);
  EXPECT_EQ("foobar_allocator", root_heap->allocator_name());
  EXPECT_EQ("", root_heap->heap_name());
  EXPECT_NE("", root_heap->GetAbsoluteName());
  EXPECT_EQ(4096u, root_heap->physical_size_in_bytes());
  EXPECT_EQ(42u, root_heap->allocated_objects_count());
  EXPECT_EQ(1000u, root_heap->allocated_objects_size_in_bytes());

  const MemoryAllocatorDump* sub_heap =
      pmd.GetAllocatorDump("foobar_allocator", "sub_heap");
  ASSERT_NE(nullptr, sub_heap);
  EXPECT_EQ("foobar_allocator", sub_heap->allocator_name());
  EXPECT_EQ("sub_heap", sub_heap->heap_name());
  EXPECT_NE("", sub_heap->GetAbsoluteName());
  EXPECT_EQ(1u, sub_heap->physical_size_in_bytes());
  EXPECT_EQ(2u, sub_heap->allocated_objects_count());
  EXPECT_EQ(3u, sub_heap->allocated_objects_size_in_bytes());

  const MemoryAllocatorDump* empty_sub_heap =
      pmd.GetAllocatorDump("foobar_allocator", "sub_heap/empty");
  ASSERT_NE(nullptr, empty_sub_heap);
  EXPECT_EQ("foobar_allocator", empty_sub_heap->allocator_name());
  EXPECT_EQ("sub_heap/empty", empty_sub_heap->heap_name());
  EXPECT_NE("", sub_heap->GetAbsoluteName());
  EXPECT_EQ(0u, empty_sub_heap->physical_size_in_bytes());
  EXPECT_EQ(0u, empty_sub_heap->allocated_objects_count());
  EXPECT_EQ(0u, empty_sub_heap->allocated_objects_size_in_bytes());

  // Check that the AsValueInfo doesn't hit any DCHECK.
  scoped_refptr<TracedValue> traced_value(new TracedValue());
  pmd.AsValueInto(traced_value.get());
}

// DEATH tests are not supported in Android / iOS.
#if !defined(NDEBUG) && !defined(OS_ANDROID) && !defined(OS_IOS)
TEST(MemoryAllocatorDumpTest, ForbidDuplicatesDeathTest) {
  FakeMemoryAllocatorDumpProvider fmadp;
  ProcessMemoryDump pmd(make_scoped_refptr(new MemoryDumpSessionState()));
  pmd.CreateAllocatorDump("foo_allocator", MemoryAllocatorDump::kRootHeap);
  pmd.CreateAllocatorDump("bar_allocator", "heap");
  ASSERT_DEATH(
      pmd.CreateAllocatorDump("foo_allocator", MemoryAllocatorDump::kRootHeap),
      "");
  ASSERT_DEATH(pmd.CreateAllocatorDump("bar_allocator", "heap"), "");
  ASSERT_DEATH(pmd.CreateAllocatorDump("", "must_have_allocator_name"), "");
}
#endif

}  // namespace trace_event
}  // namespace base
