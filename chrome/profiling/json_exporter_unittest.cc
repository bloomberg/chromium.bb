// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/json_exporter.h"

#include <sstream>

#include "base/gtest_prod_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/process/process.h"
#include "base/values.h"
#include "chrome/profiling/backtrace_storage.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profiling {

namespace {

using MemoryMap = std::vector<memory_instrumentation::mojom::VmRegionPtr>;

// Finds the first period_interval trace event in the given JSON trace.
// Returns null on failure.
const base::Value* FindFirstPeriodicInterval(const base::Value& root) {
  const base::Value* found_trace_events =
      root.FindKeyOfType("traceEvents", base::Value::Type::LIST);
  if (!found_trace_events)
    return nullptr;

  for (const base::Value& cur : found_trace_events->GetList()) {
    const base::Value* found_name =
        cur.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      return nullptr;
    if (found_name->GetString() == "periodic_interval")
      return &cur;
  }
  return nullptr;
}

// Finds the first vm region in the given periodic interval. Returns null on
// failure.
const base::Value* FindFirstRegionWithAnyName(
    const base::Value* periodic_interval) {
  const base::Value* found_args =
      periodic_interval->FindKeyOfType("args", base::Value::Type::DICTIONARY);
  if (!found_args)
    return nullptr;
  const base::Value* found_dumps =
      found_args->FindKeyOfType("dumps", base::Value::Type::DICTIONARY);
  if (!found_dumps)
    return nullptr;
  const base::Value* found_mmaps = found_dumps->FindKeyOfType(
      "process_mmaps", base::Value::Type::DICTIONARY);
  if (!found_mmaps)
    return nullptr;
  const base::Value* found_regions =
      found_mmaps->FindKeyOfType("vm_regions", base::Value::Type::LIST);
  if (!found_regions)
    return nullptr;

  for (const base::Value& cur : found_regions->GetList()) {
    const base::Value* found_name =
        cur.FindKeyOfType("mf", base::Value::Type::STRING);
    if (!found_name)
      return nullptr;
    if (found_name->GetString() != "")
      return &cur;
  }
  return nullptr;
}

}  // namespace

TEST(ProfilingJsonExporterTest, Simple) {
  BacktraceStorage backtrace_storage;

  std::vector<Address> stack1;
  stack1.push_back(Address(1234));
  stack1.push_back(Address(5678));
  const Backtrace* bt1 = backtrace_storage.Insert(std::move(stack1));

  std::vector<Address> stack2;
  stack2.push_back(Address(9012));
  const Backtrace* bt2 = backtrace_storage.Insert(std::move(stack2));

  AllocationEventSet events;
  events.insert(AllocationEvent(Address(0x1), 16, bt1));
  events.insert(AllocationEvent(Address(0x2), 32, bt2));
  events.insert(AllocationEvent(Address(0x3), 16, bt1));

  std::ostringstream stream;
  ExportAllocationEventSetToJSON(1234, events, MemoryMap(), stream, nullptr);
  std::string json = stream.str();

  // JSON should parse.
  base::JSONReader reader(base::JSON_PARSE_RFC);
  std::unique_ptr<base::Value> root = reader.ReadToValue(stream.str());
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, reader.error_code())
      << reader.GetErrorMessage();
  ASSERT_TRUE(root);

  // The trace array contains two items, a process_name one and a
  // periodic_interval one. Find the latter.
  const base::Value* periodic_interval = FindFirstPeriodicInterval(*root);
  ASSERT_TRUE(periodic_interval) << "Array contains no periodic_interval";

  const base::Value* heaps_v2 =
      periodic_interval->FindPath({"args", "dumps", "heaps_v2"});
  ASSERT_TRUE(heaps_v2);

  // Counts should be a list of two items, a 1 and a 2 (in either order). The
  // two matching 16-byte allocations should be coalesced to produce the 2.
  const base::Value* counts =
      heaps_v2->FindPath({"allocators", "malloc", "counts"});
  ASSERT_TRUE(counts);
  EXPECT_EQ(2u, counts->GetList().size());
  EXPECT_TRUE((counts->GetList()[0].GetInt() == 1 &&
               counts->GetList()[1].GetInt() == 2) ||
              (counts->GetList()[0].GetInt() == 2 &&
               counts->GetList()[1].GetInt() == 1));
}

TEST(ProfilingJsonExporterTest, MemoryMaps) {
  AllocationEventSet events;
  std::vector<memory_instrumentation::mojom::VmRegionPtr> memory_maps =
      memory_instrumentation::OSMetrics::GetProcessMemoryMaps(
          base::Process::Current().Pid());
  ASSERT_GT(memory_maps.size(), 2u);

  std::ostringstream stream;
  ExportAllocationEventSetToJSON(1234, events, memory_maps, stream, nullptr);
  std::string json = stream.str();

  // JSON should parse.
  base::JSONReader reader(base::JSON_PARSE_RFC);
  std::unique_ptr<base::Value> root = reader.ReadToValue(stream.str());
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, reader.error_code())
      << reader.GetErrorMessage();
  ASSERT_TRUE(root);

  const base::Value* periodic_interval = FindFirstPeriodicInterval(*root);
  ASSERT_TRUE(periodic_interval) << "Array contains no periodic_interval";
  const base::Value* region = FindFirstRegionWithAnyName(periodic_interval);
  ASSERT_TRUE(region) << "Array contains no named vm regions";

  const base::Value* start_address =
      region->FindKeyOfType("sa", base::Value::Type::STRING);
  ASSERT_TRUE(start_address);
  EXPECT_NE(start_address->GetString(), "");
  EXPECT_NE(start_address->GetString(), "0");

  const base::Value* size =
      region->FindKeyOfType("sz", base::Value::Type::STRING);
  ASSERT_TRUE(size);
  EXPECT_NE(size->GetString(), "");
  EXPECT_NE(size->GetString(), "0");
}

TEST(ProfilingJsonExporterTest, Metadata) {
  BacktraceStorage backtrace_storage;

  std::vector<Address> stack1;
  stack1.push_back(Address(1234));
  const Backtrace* bt1 = backtrace_storage.Insert(std::move(stack1));

  AllocationEventSet events;
  events.insert(AllocationEvent(Address(0x1), 16, bt1));

  // Generate metadata to pass in.
  std::unique_ptr<base::DictionaryValue> metadata_dict(
      new base::DictionaryValue);
  metadata_dict->SetKey("product-version", base::Value("asdf1"));
  metadata_dict->SetKey("user-agent", base::Value("\"\"\"9283hfa--+,/asdf2"));
  base::DictionaryValue metadata_dict_copy = *metadata_dict;

  std::ostringstream stream;
  ExportAllocationEventSetToJSON(1234, events, MemoryMap(), stream,
                                 std::move(metadata_dict));
  std::string json = stream.str();

  // JSON should parse.
  base::JSONReader reader(base::JSON_PARSE_RFC);
  std::unique_ptr<base::Value> root = reader.ReadToValue(stream.str());
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, reader.error_code())
      << reader.GetErrorMessage();
  ASSERT_TRUE(root);

  base::Value* found_metadatas =
      root->FindKeyOfType("metadata", base::Value::Type::DICTIONARY);
  ASSERT_TRUE(found_metadatas) << "Array contains no metadata";
  base::DictionaryValue* metadata;
  ASSERT_TRUE(found_metadatas->GetAsDictionary(&metadata));

  EXPECT_EQ(metadata_dict_copy, *metadata);
}

}  // namespace profiling
