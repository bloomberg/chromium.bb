// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string>

#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/heap_profiler_allocation_context.h"
#include "base/trace_event/heap_profiler_heap_dump_writer.h"
#include "base/trace_event/heap_profiler_stack_frame_deduplicator.h"
#include "base/trace_event/heap_profiler_type_name_deduplicator.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Define all strings once, because the deduplicator requires pointer equality,
// and string interning is unreliable.
const char kBrowserMain[] = "BrowserMain";
const char kRendererMain[] = "RendererMain";
const char kCreateWidget[] = "CreateWidget";
const char kInitialize[] = "Initialize";

const char kInt[] = "int";
const char kBool[] = "bool";
const char kString[] = "string";

}  // namespace

namespace base {
namespace trace_event {

// Asserts that an integer stored in the json as a string has the correct value.
void AssertIntEq(const DictionaryValue* entry,
                 const char* key,
                 int expected_value) {
  std::string str;
  ASSERT_TRUE(entry->GetString(key, &str));
  ASSERT_EQ(expected_value, atoi(str.c_str()));
}

scoped_ptr<Value> DumpAndReadBack(HeapDumpWriter* writer) {
  scoped_refptr<TracedValue> traced_value = writer->WriteHeapDump();
  std::string json;
  traced_value->AppendAsTraceFormat(&json);
  return JSONReader::Read(json);
}

TEST(HeapDumpWriterTest, BacktraceTypeNameTable) {
  auto sf_deduplicator = make_scoped_refptr(new StackFrameDeduplicator);
  auto tn_deduplicator = make_scoped_refptr(new TypeNameDeduplicator);
  HeapDumpWriter writer(sf_deduplicator.get(), tn_deduplicator.get());

  AllocationContext ctx = AllocationContext::Empty();
  ctx.backtrace.frames[0] = kBrowserMain;
  ctx.backtrace.frames[1] = kCreateWidget;
  ctx.type_name = kInt;

  // 10 bytes with context { type: int, bt: [BrowserMain, CreateWidget] }.
  writer.InsertAllocation(ctx, 2);
  writer.InsertAllocation(ctx, 3);
  writer.InsertAllocation(ctx, 5);

  ctx.type_name = kBool;

  // 18 bytes with context { type: bool, bt: [BrowserMain, CreateWidget] }.
  writer.InsertAllocation(ctx, 7);
  writer.InsertAllocation(ctx, 11);

  ctx.backtrace.frames[0] = kRendererMain;
  ctx.backtrace.frames[1] = kInitialize;

  // 30 bytes with context { type: bool, bt: [RendererMain, Initialize] }.
  writer.InsertAllocation(ctx, 13);
  writer.InsertAllocation(ctx, 17);

  ctx.type_name = kString;

  // 19 bytes with context { type: string, bt: [RendererMain, Initialize] }.
  writer.InsertAllocation(ctx, 19);

  // At this point the heap looks like this:
  //
  // |        | CrWidget <- BrMain | Init <- RenMain | Sum |
  // +--------+--------------------+-----------------+-----+
  // | int    |                 10 |               0 |  10 |
  // | bool   |                 18 |              30 |  48 |
  // | string |                  0 |              19 |  19 |
  // +--------+--------------------+-----------------+-----+
  // | Sum    |                 28 |              49 |  77 |

  scoped_ptr<Value> heap_dump = DumpAndReadBack(&writer);

  // The json heap dump should at least include this:
  // {
  //   "entries": [
  //     { "size": "4d" },                                    // 77 = 0x4d.
  //     { "size": "31", "bt": "id_of(Init <- RenMain)" },    // 49 = 0x31.
  //     { "size": "1c", "bt": "id_of(CrWidget <- BrMain)" }, // 28 = 0x1c.
  //     { "size": "30", "type": "id_of(bool)" },             // 48 = 0x30.
  //     { "size": "13", "type": "id_of(string)" },           // 19 = 0x13.
  //     { "size": "a", "type": "id_of(int)" }                // 10 = 0xa.
  //   ]
  // }

  // Get the indices of the backtraces and types by adding them again to the
  // deduplicator. Because they were added before, the same number is returned.
  int bt_renderer_main_initialize =
      sf_deduplicator->Insert({{kRendererMain, kInitialize}});
  int bt_browser_main_create_widget =
      sf_deduplicator->Insert({{kBrowserMain, kCreateWidget}});
  int type_id_int = tn_deduplicator->Insert(kInt);
  int type_id_bool = tn_deduplicator->Insert(kBool);
  int type_id_string = tn_deduplicator->Insert(kString);

  const DictionaryValue* dictionary;
  ASSERT_TRUE(heap_dump->GetAsDictionary(&dictionary));

  const ListValue* entries;
  ASSERT_TRUE(dictionary->GetList("entries", &entries));

  // Keep counters to verify that every entry is present exactly once.
  int x4d_seen = 0;
  int x31_seen = 0;
  int x1c_seen = 0;
  int x30_seen = 0;
  int x13_seen = 0;
  int xa_seen = 0;

  for (const Value* entry_as_value : *entries) {
    const DictionaryValue* entry;
    ASSERT_TRUE(entry_as_value->GetAsDictionary(&entry));

    // The "size" field, not to be confused with |entry->size()| which is the
    // number of elements in the dictionary.
    std::string size;
    ASSERT_TRUE(entry->GetString("size", &size));

    if (size == "4d") {
      // Total size, should not include any other field.
      ASSERT_EQ(1u, entry->size());  // Dictionary must have one element.
      x4d_seen++;
    } else if (size == "31") {
      // Entry for backtrace "Initialize <- RendererMain".
      ASSERT_EQ(2u, entry->size());  // Dictionary must have two elements.
      AssertIntEq(entry, "bt", bt_renderer_main_initialize);
      x31_seen++;
    } else if (size == "1c") {
      // Entry for backtrace "CreateWidget <- BrowserMain".
      ASSERT_EQ(2u, entry->size());  // Dictionary must have two elements.
      AssertIntEq(entry, "bt", bt_browser_main_create_widget);
      x1c_seen++;
    } else if (size == "30") {
      // Entry for type bool.
      ASSERT_EQ(2u, entry->size());  // Dictionary must have two elements.
      AssertIntEq(entry, "type", type_id_bool);
      x30_seen++;
    } else if (size == "13") {
      // Entry for type string.
      ASSERT_EQ(2u, entry->size());  // Dictionary must have two elements.
      AssertIntEq(entry, "type", type_id_string);
      x13_seen++;
    } else if (size == "a") {
      // Entry for type int.
      ASSERT_EQ(2u, entry->size());  // Dictionary must have two elements.
      AssertIntEq(entry, "type", type_id_int);
      xa_seen++;
    }
  }

  ASSERT_EQ(1, x4d_seen);
  ASSERT_EQ(1, x31_seen);
  ASSERT_EQ(1, x1c_seen);
  ASSERT_EQ(1, x30_seen);
  ASSERT_EQ(1, x13_seen);
  ASSERT_EQ(1, xa_seen);
}

// Test that the entry for the empty backtrace ends up in the json with the
// "bt" field set to the empty string. Also test that an entry for "unknown
// type" (nullptr type name) does not dereference the null pointer when writing
// the type names, and that the type ID is 0.
TEST(HeapDumpWriterTest, EmptyBacktraceIndexIsEmptyString) {
  auto sf_deduplicator = make_scoped_refptr(new StackFrameDeduplicator);
  auto tn_deduplicator = make_scoped_refptr(new TypeNameDeduplicator);
  HeapDumpWriter writer(sf_deduplicator.get(), tn_deduplicator.get());

  // A context with empty backtrace and unknown type (nullptr).
  AllocationContext ctx = AllocationContext::Empty();

  writer.InsertAllocation(ctx, 1);

  scoped_ptr<Value> heap_dump = DumpAndReadBack(&writer);

  const DictionaryValue* dictionary;
  ASSERT_TRUE(heap_dump->GetAsDictionary(&dictionary));

  const ListValue* entries;
  ASSERT_TRUE(dictionary->GetList("entries", &entries));

  int empty_backtrace_seen = 0;
  int unknown_type_seen = 0;

  for (const Value* entry_as_value : *entries) {
    const DictionaryValue* entry;
    ASSERT_TRUE(entry_as_value->GetAsDictionary(&entry));

    // Note that |entry->size()| is the number of elements in the dictionary.
    if (entry->HasKey("bt") && entry->size() == 2) {
      std::string backtrace;
      ASSERT_TRUE(entry->GetString("bt", &backtrace));
      ASSERT_EQ("", backtrace);
      empty_backtrace_seen++;
    }

    if (entry->HasKey("type") && entry->size() == 2) {
      std::string type_id;
      ASSERT_TRUE(entry->GetString("type", &type_id));
      ASSERT_EQ("0", type_id);
      unknown_type_seen++;
    }
  }

  ASSERT_EQ(1, unknown_type_seen);
  ASSERT_EQ(1, empty_backtrace_seen);
}

}  // namespace trace_event
}  // namespace base
