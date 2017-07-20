// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/json_exporter.h"

#include <sstream>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/profiling/backtrace_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profiling {

namespace {

// Finds the first period_interval trace event in the given JSON trace.
// Returns null on failure.
const base::Value* FindFirstPeriodicInterval(const base::Value& root) {
  auto found_trace_events =
      root.FindKeyOfType("traceEvents", base::Value::Type::LIST);
  if (found_trace_events == root.DictEnd())
    return nullptr;

  for (const base::Value& cur : found_trace_events->second.GetList()) {
    auto found_name = cur.FindKeyOfType("name", base::Value::Type::STRING);
    if (found_name == cur.DictEnd())
      return nullptr;
    if (found_name->second.GetString() == "periodic_interval")
      return &cur;
  }
  return nullptr;
}

const base::Value* ResolveValuePath(const base::Value& input,
                                    std::initializer_list<const char*> path) {
  const base::Value* cur = &input;
  for (const char* component : path) {
    if (!cur->is_dict())
      return nullptr;

    auto found = cur->FindKey(component);
    if (found == cur->DictEnd())
      return nullptr;
    cur = &found->second;
  }
  return cur;
}

}  // namespace

TEST(ProfilingJsonExporter, Simple) {
  BacktraceStorage backtrace_storage;

  std::vector<Address> stack1;
  stack1.push_back(Address(1234));
  stack1.push_back(Address(5678));
  BacktraceStorage::Key key1 = backtrace_storage.Insert(std::move(stack1));

  std::vector<Address> stack2;
  stack2.push_back(Address(9012));
  BacktraceStorage::Key key2 = backtrace_storage.Insert(std::move(stack2));

  AllocationEventSet events;
  events.insert(AllocationEvent(Address(0x1), 16, key1));
  events.insert(AllocationEvent(Address(0x2), 32, key2));
  events.insert(AllocationEvent(Address(0x3), 16, key1));

  std::ostringstream stream;
  ExportAllocationEventSetToJSON(1234, &backtrace_storage, events, stream);
  std::string json = stream.str();

  // JSON should parse.
  base::JSONReader reader(base::JSON_PARSE_RFC);
  std::unique_ptr<base::Value> root = reader.ReadToValue(stream.str());
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, reader.error_code())
      << reader.GetErrorMessage();
  ASSERT_TRUE(root.get());

  // The trace array contains two items, a process_name one and a
  // periodic_interval one. Find the latter.
  const base::Value* periodic_interval = FindFirstPeriodicInterval(*root);
  ASSERT_TRUE(periodic_interval) << "Array contains no periodic_interval";

  const base::Value* heaps_v2 =
      ResolveValuePath(*periodic_interval, {"args", "dumps", "heaps_v2"});
  ASSERT_TRUE(heaps_v2);

  // Counts should be a list of two items, a 1 and a 2 (in either order). The
  // two matching 16-byte allocations should be coalesced to produce the 2.
  const base::Value* counts =
      ResolveValuePath(*heaps_v2, {"allocators", "malloc", "counts"});
  ASSERT_TRUE(counts);
  EXPECT_EQ(2u, counts->GetList().size());
  EXPECT_TRUE((counts->GetList()[0].GetInt() == 1 &&
               counts->GetList()[1].GetInt() == 2) ||
              (counts->GetList()[0].GetInt() == 2 &&
               counts->GetList()[1].GetInt() == 1));
}

}  // namespace profiling
