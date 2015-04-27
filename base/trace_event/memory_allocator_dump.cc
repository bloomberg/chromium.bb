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

// static
const char MemoryAllocatorDump::kRootHeap[] = "";

// static
std::string MemoryAllocatorDump::GetAbsoluteName(
    const std::string& allocator_name,
    const std::string& heap_name) {
  return allocator_name + (heap_name == kRootHeap ? "" : "/" + heap_name);
}

MemoryAllocatorDump::MemoryAllocatorDump(const std::string& allocator_name,
                                         const std::string& heap_name,
                                         ProcessMemoryDump* process_memory_dump)
    : allocator_name_(allocator_name),
      heap_name_(heap_name),
      process_memory_dump_(process_memory_dump),
      physical_size_in_bytes_(0),
      allocated_objects_count_(0),
      allocated_objects_size_in_bytes_(0) {
  // The allocator name cannot be empty or contain slash separators.
  DCHECK(!allocator_name.empty());
  DCHECK_EQ(std::string::npos, allocator_name.find_first_of('/'));

  // The heap_name can be empty and contain slash separator, but not
  // leading or trailing ones.
  DCHECK(heap_name.empty() ||
         (heap_name[0] != '/' && *heap_name.rbegin() != '/'));

  // Dots are not allowed anywhere as the underlying base::DictionaryValue
  // would treat them magically and split in sub-nodes, which is not intended.
  DCHECK_EQ(std::string::npos, allocator_name.find_first_of('.'));
  DCHECK_EQ(std::string::npos, heap_name.find_first_of('.'));
}

MemoryAllocatorDump::~MemoryAllocatorDump() {
}

void MemoryAllocatorDump::SetAttribute(const std::string& name, int value) {
  DCHECK(GetAttributesTypeInfo().Exists(allocator_name_, name))
      << "attribute '" << name << "' not declared."
      << "See MemoryDumpProvider.DeclareAllocatorAttribute()";
  attributes_values_.SetInteger(name, value);
}

std::string MemoryAllocatorDump::GetAbsoluteName() const {
  return GetAbsoluteName(allocator_name_, heap_name_);
}

int MemoryAllocatorDump::GetIntegerAttribute(const std::string& name) const {
  int value = -1;
  bool res = attributes_values_.GetInteger(name, &value);
  DCHECK(res) << "Attribute '" << name << "' not found";
  return value;
}

void MemoryAllocatorDump::AsValueInto(TracedValue* value) const {
  static const char kHexFmt[] = "%" PRIx64;

  value->BeginDictionary(GetAbsoluteName().c_str());
  value->BeginDictionary("attrs");

  // TODO(primiano): these hard-coded types are temporary to transition to the
  // new generalized attribute format. This code will be refactored by the end
  // of May 2015.
  value->BeginDictionary("outer_size");
  value->SetString("type", "scalar");
  value->SetString("units", "bytes");
  value->SetString("value", StringPrintf(kHexFmt, physical_size_in_bytes_));
  value->EndDictionary();

  value->BeginDictionary("inner_size");
  value->SetString("type", "scalar");
  value->SetString("units", "bytes");
  value->SetString("value",
                   StringPrintf(kHexFmt, allocated_objects_size_in_bytes_));
  value->EndDictionary();

  value->BeginDictionary("objects_count");
  value->SetString("type", "scalar");
  value->SetString("units", "objects");
  value->SetString("value", StringPrintf(kHexFmt, allocated_objects_count_));
  value->EndDictionary();

  // Copy all the extra attributes.
  for (DictionaryValue::Iterator it(attributes_values_); !it.IsAtEnd();
       it.Advance()) {
    const std::string& attr_name = it.key();
    const Value& attr_value = it.value();
    value->BeginDictionary(attr_name.c_str());
    value->SetValue("value", attr_value.DeepCopy());

    const std::string& attr_type =
        GetAttributesTypeInfo().Get(allocator_name_, attr_name);
    DCHECK(!attr_type.empty());
    value->SetString("type", "scalar");
    value->SetString("units", attr_type);

    value->EndDictionary();  // "arg_name": { "type": "...", "value": "..." }
  }

  value->EndDictionary();  // "attrs": { ... }
  value->EndDictionary();  // "allocator_name/heap_subheap": { ... }
}

const MemoryAllocatorAttributesTypeInfo&
MemoryAllocatorDump::GetAttributesTypeInfo() const {
  return process_memory_dump_->session_state()->allocators_attributes_type_info;
}

}  // namespace trace_event
}  // namespace base
