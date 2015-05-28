// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_dump.h"

#include "base/memory/scoped_ptr.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/trace_event_argument.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

TEST(ProcessMemoryDumpTest, Clear) {
  scoped_ptr<ProcessMemoryDump> pmd1(new ProcessMemoryDump(nullptr));
  pmd1->CreateAllocatorDump("mad1");
  pmd1->CreateAllocatorDump("mad2");
  ASSERT_FALSE(pmd1->allocator_dumps().empty());

  pmd1->process_totals()->set_resident_set_bytes(42);
  pmd1->set_has_process_totals();

  pmd1->process_mmaps()->AddVMRegion({0});
  pmd1->set_has_process_mmaps();

  pmd1->AddOwnershipEdge(MemoryAllocatorDumpGuid(42),
                         MemoryAllocatorDumpGuid(4242));

  pmd1->Clear();
  ASSERT_TRUE(pmd1->allocator_dumps().empty());
  ASSERT_TRUE(pmd1->allocator_dumps_edges().empty());
  ASSERT_EQ(nullptr, pmd1->GetAllocatorDump("mad1"));
  ASSERT_EQ(nullptr, pmd1->GetAllocatorDump("mad2"));
  ASSERT_FALSE(pmd1->has_process_totals());
  ASSERT_FALSE(pmd1->has_process_mmaps());
  ASSERT_TRUE(pmd1->process_mmaps()->vm_regions().empty());

  // Check that calling AsValueInto() doesn't cause a crash.
  scoped_refptr<TracedValue> traced_value(new TracedValue());
  pmd1->AsValueInto(traced_value.get());

  // Check that the pmd can be reused and behaves as expected.
  pmd1->CreateAllocatorDump("mad1");
  pmd1->CreateAllocatorDump("mad3");
  ASSERT_EQ(2u, pmd1->allocator_dumps().size());
  ASSERT_NE(static_cast<MemoryAllocatorDump*>(nullptr),
            pmd1->GetAllocatorDump("mad1"));
  ASSERT_EQ(nullptr, pmd1->GetAllocatorDump("mad2"));
  ASSERT_NE(static_cast<MemoryAllocatorDump*>(nullptr),
            pmd1->GetAllocatorDump("mad3"));

  traced_value = new TracedValue();
  pmd1->AsValueInto(traced_value.get());

  pmd1.reset();
}

TEST(ProcessMemoryDumpTest, TakeAllDumpsFrom) {
  scoped_refptr<TracedValue> traced_value(new TracedValue());

  scoped_ptr<ProcessMemoryDump> pmd1(new ProcessMemoryDump(nullptr));
  auto mad1_1 = pmd1->CreateAllocatorDump("pmd1/mad1");
  auto mad1_2 = pmd1->CreateAllocatorDump("pmd1/mad2");
  pmd1->AddOwnershipEdge(mad1_1->guid(), mad1_2->guid());

  scoped_ptr<ProcessMemoryDump> pmd2(new ProcessMemoryDump(nullptr));
  auto mad2_1 = pmd2->CreateAllocatorDump("pmd2/mad1");
  auto mad2_2 = pmd2->CreateAllocatorDump("pmd2/mad2");
  pmd1->AddOwnershipEdge(mad2_1->guid(), mad2_2->guid());

  pmd1->TakeAllDumpsFrom(pmd2.get());

  // Make sure that pmd2 is empty but still usable after it has been emptied.
  ASSERT_TRUE(pmd2->allocator_dumps().empty());
  ASSERT_TRUE(pmd2->allocator_dumps_edges().empty());
  pmd2->CreateAllocatorDump("pmd2/this_mad_stays_with_pmd2");
  ASSERT_EQ(1u, pmd2->allocator_dumps().size());
  ASSERT_EQ(1u, pmd2->allocator_dumps().count("pmd2/this_mad_stays_with_pmd2"));
  pmd2->AddOwnershipEdge(MemoryAllocatorDumpGuid(42),
                         MemoryAllocatorDumpGuid(4242));

  // Check that calling AsValueInto() doesn't cause a crash.
  pmd2->AsValueInto(traced_value.get());

  // Free the |pmd2| to check that the memory ownership of the two MAD(s)
  // has been transferred to |pmd1|.
  pmd2.reset();

  // Now check that |pmd1| has been effectively merged.
  ASSERT_EQ(4u, pmd1->allocator_dumps().size());
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd1/mad1"));
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd1/mad2"));
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd2/mad1"));
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd1/mad2"));
  ASSERT_EQ(2u, pmd1->allocator_dumps_edges().size());

  // Check that calling AsValueInto() doesn't cause a crash.
  traced_value = new TracedValue();
  pmd1->AsValueInto(traced_value.get());

  pmd1.reset();
}

}  // namespace trace_event
}  // namespace base
