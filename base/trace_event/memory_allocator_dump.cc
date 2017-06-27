// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"

#include "base/format_macros.h"
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
      attributes_(new TracedValue),
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
  string_conversion_buffer_.reserve(16);
}

MemoryAllocatorDump::~MemoryAllocatorDump() {
}

void MemoryAllocatorDump::AddScalar(const char* name,
                                    const char* units,
                                    uint64_t value) {
  if (strcmp(kNameSize, name) == 0)
    size_ = value;
  SStringPrintf(&string_conversion_buffer_, "%" PRIx64, value);
  attributes_->BeginDictionary(name);
  attributes_->SetString("type", kTypeScalar);
  attributes_->SetString("units", units);
  attributes_->SetString("value", string_conversion_buffer_);
  attributes_->EndDictionary();
}

void MemoryAllocatorDump::AddScalarF(const char* name,
                                     const char* units,
                                     double value) {
  attributes_->BeginDictionary(name);
  attributes_->SetString("type", kTypeScalar);
  attributes_->SetString("units", units);
  attributes_->SetDouble("value", value);
  attributes_->EndDictionary();
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

  attributes_->BeginDictionary(name);
  attributes_->SetString("type", kTypeString);
  attributes_->SetString("units", units);
  attributes_->SetString("value", value);
  attributes_->EndDictionary();
}

void MemoryAllocatorDump::AsValueInto(TracedValue* value) const {
  value->BeginDictionaryWithCopiedName(absolute_name_);
  value->SetString("guid", guid_.ToString());
  value->SetValue("attrs", *attributes_);
  if (flags_)
    value->SetInteger("flags", flags_);
  value->EndDictionary();  // "allocator_name/heap_subheap": { ... }
}

}  // namespace trace_event
}  // namespace base
