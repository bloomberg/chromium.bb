// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
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
    MemoryAllocatorDump* root_heap =
        pmd->CreateAllocatorDump("foobar_allocator");

    root_heap->AddScalar(MemoryAllocatorDump::kNameOuterSize,
                         MemoryAllocatorDump::kUnitsBytes, 4096);
    root_heap->AddScalar(MemoryAllocatorDump::kNameInnerSize,
                         MemoryAllocatorDump::kUnitsBytes, 1000);
    root_heap->AddScalar(MemoryAllocatorDump::kNameObjectsCount,
                         MemoryAllocatorDump::kUnitsObjects, 42);
    root_heap->AddScalar("attr1", "units1", 1234);
    root_heap->AddString("attr2", "units2", "string_value");

    MemoryAllocatorDump* sub_heap =
        pmd->CreateAllocatorDump("foobar_allocator/sub_heap");
    sub_heap->AddScalar(MemoryAllocatorDump::kNameOuterSize,
                        MemoryAllocatorDump::kUnitsBytes, 1);
    sub_heap->AddScalar(MemoryAllocatorDump::kNameInnerSize,
                        MemoryAllocatorDump::kUnitsBytes, 2);
    sub_heap->AddScalar(MemoryAllocatorDump::kNameObjectsCount,
                        MemoryAllocatorDump::kUnitsObjects, 3);

    pmd->CreateAllocatorDump("foobar_allocator/sub_heap/empty");
    // Leave the rest of sub heap deliberately uninitialized, to check that
    // CreateAllocatorDump returns a properly zero-initialized object.

    return true;
  }
};

void CheckAttribute(const MemoryAllocatorDump* dump,
                    const std::string& name,
                    const char* expected_type,
                    const char* expected_units,
                    const std::string& expected_value) {
  const char* attr_type;
  const char* attr_units;
  const Value* attr_value;
  std::string attr_str_value;
  bool res = dump->Get(name, &attr_type, &attr_units, &attr_value);
  EXPECT_TRUE(res);
  if (!res)
    return;
  EXPECT_EQ(expected_type, std::string(attr_type));
  EXPECT_EQ(expected_units, std::string(attr_units));
  EXPECT_TRUE(attr_value->GetAsString(&attr_str_value));
  EXPECT_EQ(expected_value, attr_str_value);
}

void CheckAttribute(const MemoryAllocatorDump* dump,
                    const std::string& name,
                    const char* expected_type,
                    const char* expected_units,
                    uint64 expected_value) {
  CheckAttribute(dump, name, expected_type, expected_units,
                 StringPrintf("%" PRIx64, expected_value));
}
}  // namespace

TEST(MemoryAllocatorDumpTest, DumpIntoProcessMemoryDump) {
  FakeMemoryAllocatorDumpProvider fmadp;
  ProcessMemoryDump pmd(make_scoped_refptr(new MemoryDumpSessionState()));

  fmadp.OnMemoryDump(&pmd);

  ASSERT_EQ(3u, pmd.allocator_dumps().size());

  const MemoryAllocatorDump* root_heap =
      pmd.GetAllocatorDump("foobar_allocator");
  ASSERT_NE(nullptr, root_heap);
  EXPECT_EQ("foobar_allocator", root_heap->absolute_name());
  CheckAttribute(root_heap, MemoryAllocatorDump::kNameOuterSize,
                 MemoryAllocatorDump::kTypeScalar,
                 MemoryAllocatorDump::kUnitsBytes, 4096);
  CheckAttribute(root_heap, MemoryAllocatorDump::kNameInnerSize,
                 MemoryAllocatorDump::kTypeScalar,
                 MemoryAllocatorDump::kUnitsBytes, 1000);
  CheckAttribute(root_heap, MemoryAllocatorDump::kNameObjectsCount,
                 MemoryAllocatorDump::kTypeScalar,
                 MemoryAllocatorDump::kUnitsObjects, 42);
  CheckAttribute(root_heap, "attr1", MemoryAllocatorDump::kTypeScalar, "units1",
                 1234);
  CheckAttribute(root_heap, "attr2", MemoryAllocatorDump::kTypeString, "units2",
                 "string_value");

  const MemoryAllocatorDump* sub_heap =
      pmd.GetAllocatorDump("foobar_allocator/sub_heap");
  ASSERT_NE(nullptr, sub_heap);
  EXPECT_EQ("foobar_allocator/sub_heap", sub_heap->absolute_name());
  CheckAttribute(sub_heap, MemoryAllocatorDump::kNameOuterSize,
                 MemoryAllocatorDump::kTypeScalar,
                 MemoryAllocatorDump::kUnitsBytes, 1);
  CheckAttribute(sub_heap, MemoryAllocatorDump::kNameInnerSize,
                 MemoryAllocatorDump::kTypeScalar,
                 MemoryAllocatorDump::kUnitsBytes, 2);
  CheckAttribute(sub_heap, MemoryAllocatorDump::kNameObjectsCount,
                 MemoryAllocatorDump::kTypeScalar,
                 MemoryAllocatorDump::kUnitsObjects, 3);

  const MemoryAllocatorDump* empty_sub_heap =
      pmd.GetAllocatorDump("foobar_allocator/sub_heap/empty");
  ASSERT_NE(nullptr, empty_sub_heap);
  EXPECT_EQ("foobar_allocator/sub_heap/empty", empty_sub_heap->absolute_name());
  ASSERT_FALSE(empty_sub_heap->Get(MemoryAllocatorDump::kNameOuterSize, nullptr,
                                   nullptr, nullptr));
  ASSERT_FALSE(empty_sub_heap->Get(MemoryAllocatorDump::kNameInnerSize, nullptr,
                                   nullptr, nullptr));
  ASSERT_FALSE(empty_sub_heap->Get(MemoryAllocatorDump::kNameObjectsCount,
                                   nullptr, nullptr, nullptr));

  // Check that the AsValueInfo doesn't hit any DCHECK.
  scoped_refptr<TracedValue> traced_value(new TracedValue());
  pmd.AsValueInto(traced_value.get());
}

// DEATH tests are not supported in Android / iOS.
#if !defined(NDEBUG) && !defined(OS_ANDROID) && !defined(OS_IOS)
TEST(MemoryAllocatorDumpTest, ForbidDuplicatesDeathTest) {
  FakeMemoryAllocatorDumpProvider fmadp;
  ProcessMemoryDump pmd(make_scoped_refptr(new MemoryDumpSessionState()));
  pmd.CreateAllocatorDump("foo_allocator");
  pmd.CreateAllocatorDump("bar_allocator/heap");
  ASSERT_DEATH(pmd.CreateAllocatorDump("foo_allocator"), "");
  ASSERT_DEATH(pmd.CreateAllocatorDump("bar_allocator/heap"), "");
  ASSERT_DEATH(pmd.CreateAllocatorDump(""), "");
}
#endif

}  // namespace trace_event
}  // namespace base
