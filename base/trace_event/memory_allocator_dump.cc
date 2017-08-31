// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"

namespace base {
namespace trace_event {

const char MemoryAllocatorDump::kNameSize[] = "size";
const char MemoryAllocatorDump::kNameObjectCount[] = "object_count";
const char MemoryAllocatorDump::kTypeScalar[] = "scalar";
const char MemoryAllocatorDump::kTypeString[] = "string";
const char MemoryAllocatorDump::kUnitsBytes[] = "bytes";
const char MemoryAllocatorDump::kUnitsObjects[] = "objects";

// static
MemoryAllocatorDumpGuid MemoryAllocatorDump::GetDumpIdFromName(
    const std::string& absolute_name) {
  return MemoryAllocatorDumpGuid(StringPrintf(
      "%d:%s", TraceLog::GetInstance()->process_id(), absolute_name.c_str()));
}

MemoryAllocatorDump::MemoryAllocatorDump(const std::string& absolute_name,
                                         ProcessMemoryDump* process_memory_dump,
                                         const MemoryAllocatorDumpGuid& guid)
    : absolute_name_(absolute_name),
      process_memory_dump_(process_memory_dump),
      guid_(guid),
      flags_(Flags::DEFAULT),
      size_(0) {
  // The |absolute_name| cannot be empty.
  DCHECK(!absolute_name.empty());

  // The |absolute_name| can contain slash separator, but not leading or
  // trailing ones.
  DCHECK(absolute_name[0] != '/' && *absolute_name.rbegin() != '/');
}

// If the caller didn't provide a guid, make one up by hashing the
// absolute_name with the current PID.
// Rationale: |absolute_name| is already supposed to be unique within a
// process, the pid will make it unique among all processes.
MemoryAllocatorDump::MemoryAllocatorDump(const std::string& absolute_name,
                                         ProcessMemoryDump* process_memory_dump)
    : MemoryAllocatorDump(absolute_name,
                          process_memory_dump,
                          GetDumpIdFromName(absolute_name)) {
}

MemoryAllocatorDump::~MemoryAllocatorDump() {
}

void MemoryAllocatorDump::AddScalar(const char* name,
                                    const char* units,
                                    uint64_t value) {
  if (strcmp(kNameSize, name) == 0)
    size_ = value;
  entries_.emplace_back(name, units, value);
}

void MemoryAllocatorDump::AddString(const char* name,
                                    const char* units,
                                    const std::string& value) {
  // String attributes are disabled in background mode.
  if (process_memory_dump_->dump_args().level_of_detail ==
      MemoryDumpLevelOfDetail::BACKGROUND) {
    NOTREACHED();
    return;
  }
  entries_.emplace_back(name, units, value);
}

void MemoryAllocatorDump::DumpAttributes(TracedValue* value) const {
  std::string string_conversion_buffer;

  for (const Entry& entry : entries_) {
    value->BeginDictionaryWithCopiedName(entry.name);
    switch (entry.entry_type) {
      case Entry::kUint64:
        SStringPrintf(&string_conversion_buffer, "%" PRIx64,
                      entry.value_uint64);
        value->SetString("type", kTypeScalar);
        value->SetString("units", entry.units);
        value->SetString("value", string_conversion_buffer);
        break;
      case Entry::kString:
        value->SetString("type", kTypeString);
        value->SetString("units", entry.units);
        value->SetString("value", entry.value_string);
        break;
    }
    value->EndDictionary();
  }
}

void MemoryAllocatorDump::AsValueInto(TracedValue* value) const {
  value->BeginDictionaryWithCopiedName(absolute_name_);
  value->SetString("guid", guid_.ToString());
  value->BeginDictionary("attrs");
  DumpAttributes(value);
  value->EndDictionary();  // "attrs": { ... }
  if (flags_)
    value->SetInteger("flags", flags_);
  value->EndDictionary();  // "allocator_name/heap_subheap": { ... }
}

std::unique_ptr<TracedValue> MemoryAllocatorDump::attributes_for_testing()
    const {
  std::unique_ptr<TracedValue> attributes = base::MakeUnique<TracedValue>();
  DumpAttributes(attributes.get());
  return attributes;
}

MemoryAllocatorDump::Entry::Entry(Entry&& other) = default;

MemoryAllocatorDump::Entry::Entry(std::string name,
                                  std::string units,
                                  uint64_t value)
    : name(name), units(units), entry_type(kUint64), value_uint64(value) {}
MemoryAllocatorDump::Entry::Entry(std::string name,
                                  std::string units,
                                  std::string value)
    : name(name), units(units), entry_type(kString), value_string(value) {}

bool MemoryAllocatorDump::Entry::operator==(const Entry& rhs) const {
  if (!(name == rhs.name && units == rhs.units && entry_type == rhs.entry_type))
    return false;
  switch (entry_type) {
    case EntryType::kUint64:
      return value_uint64 == rhs.value_uint64;
    case EntryType::kString:
      return value_string == rhs.value_string;
  }
  NOTREACHED();
  return false;
}

}  // namespace trace_event
}  // namespace base
