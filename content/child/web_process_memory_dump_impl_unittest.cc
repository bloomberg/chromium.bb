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
// TODO(primiano): Temporarily disabled on 11/06/15 to unblock the blink roll.
TEST(WebProcessMemoryDumpImplTest, DISABLED_IntegrationTest) {
  scoped_refptr<base::trace_event::TracedValue> traced_value(
      new base::trace_event::TracedValue());

  scoped_ptr<WebProcessMemoryDumpImpl> wpmd1(new WebProcessMemoryDumpImpl());
  auto wmad1 = wpmd1->createMemoryAllocatorDump("1/1");
  auto wmad2 = wpmd1->createMemoryAllocatorDump("1/2");
  ASSERT_EQ(wmad1, wpmd1->getMemoryAllocatorDump("1/1"));
  ASSERT_EQ(wmad2, wpmd1->getMemoryAllocatorDump("1/2"));

  scoped_ptr<WebProcessMemoryDumpImpl> wpmd2(new WebProcessMemoryDumpImpl());
  wpmd2->createMemoryAllocatorDump("2/1");
  wpmd2->createMemoryAllocatorDump("2/2");

  wpmd1->takeAllDumpsFrom(wpmd2.get());

  // Make sure that wpmd2 still owns its own PMD, even if empty.
  ASSERT_NE(static_cast<base::trace_event::ProcessMemoryDump*>(nullptr),
            wpmd2->process_memory_dump_);
  ASSERT_EQ(wpmd2->owned_process_memory_dump_.get(),
            wpmd2->process_memory_dump());
  ASSERT_TRUE(wpmd2->process_memory_dump()->allocator_dumps().empty());

  // Make sure that wpmd2 is still usable after it has been emptied.
  auto wmad = wpmd2->createMemoryAllocatorDump("2/new");
  wmad->AddScalar("attr_name", "bytes", 42);
  wmad->AddScalarF("attr_name_2", "rate", 42.0f);
  ASSERT_EQ(1u, wpmd2->process_memory_dump()->allocator_dumps().size());
  auto mad = wpmd2->process_memory_dump()->GetAllocatorDump("2/new");
  ASSERT_NE(static_cast<base::trace_event::MemoryAllocatorDump*>(nullptr), mad);
  ASSERT_EQ(wmad, wpmd2->getMemoryAllocatorDump("2/new"));

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
  wpmd2->process_memory_dump()->AsValueInto(traced_value.get());

  // Free the |wpmd2| to check that the memory ownership of the two MAD(s)
  // has been transferred to |wpmd1|.
  wpmd2.reset();

  // Now check that |wpmd1| has been effectively merged.
  ASSERT_EQ(4u, wpmd1->process_memory_dump()->allocator_dumps().size());
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("1/1"));
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("1/2"));
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("2/1"));
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("1/2"));

  // Check that also the WMAD wrappers got merged.
  blink::WebMemoryAllocatorDump* null_wmad = nullptr;
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("1/1"));
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("1/2"));
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("2/1"));
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("2/2"));

  // Check that AsValueInto() doesn't cause a crash.
  traced_value = new base::trace_event::TracedValue();
  wpmd1->process_memory_dump()->AsValueInto(traced_value.get());

  // Check that clear() actually works.
  wpmd1->clear();
  ASSERT_TRUE(wpmd1->process_memory_dump()->allocator_dumps().empty());
  ASSERT_EQ(nullptr, wpmd1->process_memory_dump()->GetAllocatorDump("1/1"));
  ASSERT_EQ(nullptr, wpmd1->process_memory_dump()->GetAllocatorDump("2/1"));

  // Check that AsValueInto() doesn't cause a crash.
  traced_value = new base::trace_event::TracedValue();
  wpmd1->process_memory_dump()->AsValueInto(traced_value.get());

  wpmd1.reset();
}

}  // namespace content
