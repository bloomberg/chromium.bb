// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_process_memory_dump_impl.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "content/child/web_memory_allocator_dump_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Tests that the Chromium<>Blink plumbing that exposes the MemoryInfra classes
// behaves correctly, performs the right transfers of memory ownerships and
// doesn't leak objects.
TEST(WebProcessMemoryDumpImplTest, IntegrationTest) {
  scoped_refptr<base::trace_event::TracedValue> traced_value(
      new base::trace_event::TracedValue());

  scoped_ptr<WebProcessMemoryDumpImpl> pmd1(new WebProcessMemoryDumpImpl());
  pmd1->createMemoryAllocatorDump("pmd1/1");
  pmd1->createMemoryAllocatorDump("pmd1/2");

  scoped_ptr<WebProcessMemoryDumpImpl> pmd2(new WebProcessMemoryDumpImpl());
  pmd2->createMemoryAllocatorDump("pmd2/1");
  pmd2->createMemoryAllocatorDump("pmd2/2");

  pmd1->takeAllDumpsFrom(pmd2.get());

  // Make sure that pmd2 still owns its own PMD, even if empty.
  ASSERT_NE(static_cast<base::trace_event::ProcessMemoryDump*>(nullptr),
            pmd2->process_memory_dump_);
  ASSERT_EQ(pmd2->owned_process_memory_dump_.get(),
            pmd2->process_memory_dump());
  ASSERT_TRUE(pmd2->process_memory_dump()->allocator_dumps().empty());

  // Make sure that pmd2 is still usable after it has been emptied.
  auto wmad = pmd2->createMemoryAllocatorDump("pmd2/new");
  wmad->AddScalar("attr_name", "bytes", 42);
  wmad->AddScalarF("attr_name_2", "rate", 42.0f);
  ASSERT_EQ(1u, pmd2->process_memory_dump()->allocator_dumps().size());
  auto mad = pmd2->process_memory_dump()->GetAllocatorDump("pmd2/new");
  ASSERT_NE(static_cast<base::trace_event::MemoryAllocatorDump*>(nullptr), mad);
  // Check that the attributes are propagated correctly.
  const char* attr_type = nullptr;
  const char* attr_units = nullptr;
  const base::Value* attr_value = nullptr;
  bool has_attr = mad->Get("attr_name", &attr_type, &attr_units, &attr_value);
  ASSERT_TRUE(has_attr);
  ASSERT_STREQ(base::trace_event::MemoryAllocatorDump::kTypeScalar, attr_type);
  ASSERT_STREQ("bytes", attr_units);
  ASSERT_NE(static_cast<base::Value*>(nullptr), attr_value);
  has_attr = mad->Get("attr_name_2", &attr_type, &attr_units, &attr_value);
  ASSERT_TRUE(has_attr);
  ASSERT_STREQ(base::trace_event::MemoryAllocatorDump::kTypeScalar, attr_type);
  ASSERT_STREQ("rate", attr_units);
  ASSERT_NE(static_cast<base::Value*>(nullptr), attr_value);

  // Check that AsValueInto() doesn't cause a crash.
  pmd2->process_memory_dump()->AsValueInto(traced_value.get());

  // Free the |pmd2| to check that the memory ownership of the two MAD(s)
  // has been transferred to |pmd1|.
  pmd2.reset();

  // Now check that |pmd1| has been effectively merged.
  ASSERT_EQ(4u, pmd1->process_memory_dump()->allocator_dumps().size());
  ASSERT_EQ(1u, pmd1->process_memory_dump()->allocator_dumps().count("pmd1/1"));
  ASSERT_EQ(1u, pmd1->process_memory_dump()->allocator_dumps().count("pmd1/2"));
  ASSERT_EQ(1u, pmd1->process_memory_dump()->allocator_dumps().count("pmd2/1"));
  ASSERT_EQ(1u, pmd1->process_memory_dump()->allocator_dumps().count("pmd1/2"));

  // Check that AsValueInto() doesn't cause a crash.
  traced_value = new base::trace_event::TracedValue();
  pmd1->process_memory_dump()->AsValueInto(traced_value.get());

  // Check that clear() actually works.
  pmd1->clear();
  ASSERT_TRUE(pmd1->process_memory_dump()->allocator_dumps().empty());
  ASSERT_EQ(nullptr, pmd1->process_memory_dump()->GetAllocatorDump("pmd1/1"));
  ASSERT_EQ(nullptr, pmd1->process_memory_dump()->GetAllocatorDump("pmd2/1"));

  // Check that AsValueInto() doesn't cause a crash.
  traced_value = new base::trace_event::TracedValue();
  pmd1->process_memory_dump()->AsValueInto(traced_value.get());

  pmd1.reset();
}

}  // namespace content
