// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"

#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

namespace {
class FakeMemoryAllocatorDumpProvider : public MemoryDumpProvider {
 public:
  FakeMemoryAllocatorDumpProvider() {
    DeclareAllocatorAttribute({"attr1", "count"});
    DeclareAllocatorAttribute({"attr2", "bytes"});
  }

  bool DumpInto(ProcessMemoryDump* pmd) override {
    MemoryAllocatorDump* mad_foo = pmd->CreateAllocatorDump("foo");
    mad_foo->set_physical_size_in_bytes(4096);
    mad_foo->set_allocated_objects_count(42);
    mad_foo->set_allocated_objects_size_in_bytes(1000);
    mad_foo->SetExtraAttribute("attr1", 1234);
    mad_foo->SetExtraAttribute("attr2", 99);

    MemoryAllocatorDump* mad_bar = pmd->CreateAllocatorDump("foo/bar", mad_foo);
    mad_bar->set_physical_size_in_bytes(1);
    mad_bar->set_allocated_objects_count(2);
    mad_bar->set_allocated_objects_size_in_bytes(3);

    pmd->CreateAllocatorDump("baz");
    // Leave the rest of |baz| deliberately uninitialized, to check that
    // CreateAllocatorDump returns a properly zero-initialized object.

    return true;
  }

  const char* GetFriendlyName() const override { return "mock_allocator"; }
};
}  // namespace

TEST(MemoryAllocatorDumpTest, DumpIntoProcessMemoryDump) {
  FakeMemoryAllocatorDumpProvider fmadp;
  ProcessMemoryDump pmd;

  fmadp.DumpInto(&pmd);

  ASSERT_EQ(3u, pmd.allocator_dumps().size());

  const MemoryAllocatorDump* mad_foo = pmd.GetAllocatorDump("foo");
  ASSERT_NE(nullptr, mad_foo);
  EXPECT_EQ("foo", mad_foo->name());
  ASSERT_EQ(nullptr, mad_foo->parent());
  EXPECT_EQ(4096u, mad_foo->physical_size_in_bytes());
  EXPECT_EQ(42u, mad_foo->allocated_objects_count());
  EXPECT_EQ(1000u, mad_foo->allocated_objects_size_in_bytes());

  // Check the extra attributes of |mad_foo|.
  EXPECT_EQ(1234, mad_foo->GetExtraIntegerAttribute("attr1"));
  EXPECT_EQ(99, mad_foo->GetExtraIntegerAttribute("attr2"));

  const MemoryAllocatorDump* mad_bar = pmd.GetAllocatorDump("foo/bar");
  ASSERT_NE(nullptr, mad_bar);
  EXPECT_EQ("foo/bar", mad_bar->name());
  ASSERT_EQ(mad_foo, mad_bar->parent());
  EXPECT_EQ(1u, mad_bar->physical_size_in_bytes());
  EXPECT_EQ(2u, mad_bar->allocated_objects_count());
  EXPECT_EQ(3u, mad_bar->allocated_objects_size_in_bytes());

  const MemoryAllocatorDump* mad_baz = pmd.GetAllocatorDump("baz");
  ASSERT_NE(nullptr, mad_baz);
  EXPECT_EQ("baz", mad_baz->name());
  ASSERT_EQ(nullptr, mad_baz->parent());
  EXPECT_EQ(0u, mad_baz->physical_size_in_bytes());
  EXPECT_EQ(0u, mad_baz->allocated_objects_count());
  EXPECT_EQ(0u, mad_baz->allocated_objects_size_in_bytes());
}

// DEATH tests are not supported in Android / iOS.
#if !defined(NDEBUG) && !defined(OS_ANDROID) && !defined(OS_IOS)
TEST(MemoryAllocatorDumpTest, ForbidDuplicatesDeathTest) {
  FakeMemoryAllocatorDumpProvider fmadp;
  ProcessMemoryDump pmd;
  pmd.CreateAllocatorDump("dump_1");
  pmd.CreateAllocatorDump("dump_2");
  ASSERT_DEATH(pmd.CreateAllocatorDump("dump_1"), "");
}
#endif

}  // namespace trace_event
}  // namespace base
