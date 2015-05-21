// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_dump.h"

#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_event_argument.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

TEST(ProcessMemoryDumpTest, TakeAllDumpsFrom) {
  scoped_refptr<TracedValue> traced_value(new TracedValue());

  scoped_ptr<ProcessMemoryDump> pmd1(new ProcessMemoryDump(nullptr));
  pmd1->CreateAllocatorDump("pmd1/mad1");
  pmd1->CreateAllocatorDump("pmd1/mad2");

  scoped_ptr<ProcessMemoryDump> pmd2(new ProcessMemoryDump(nullptr));
  pmd2->CreateAllocatorDump("pmd2/mad1");
  pmd2->CreateAllocatorDump("pmd2/mad2");

  pmd1->TakeAllDumpsFrom(pmd2.get());

  // Make sure that pmd2 is empty but still usable after it has been emptied.
  ASSERT_TRUE(pmd2->allocator_dumps().empty());
  pmd2->CreateAllocatorDump("pmd2/this_mad_stays_with_pmd2");
  ASSERT_EQ(1u, pmd2->allocator_dumps().size());
  ASSERT_EQ(1u, pmd2->allocator_dumps().count("pmd2/this_mad_stays_with_pmd2"));

  // Check that the AsValueInto doesn't cause a crash.
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

  // Check that the AsValueInto doesn't cause a crash.
  traced_value = new TracedValue();
  pmd1->AsValueInto(traced_value.get());

  pmd1.reset();
}

}  // namespace trace_event
}  // namespace base
