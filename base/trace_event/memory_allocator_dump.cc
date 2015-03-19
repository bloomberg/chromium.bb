// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_attributes.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"

namespace base {
namespace trace_event {

MemoryAllocatorDump::MemoryAllocatorDump(const std::string& name,
                                         MemoryAllocatorDump* parent)
    : name_(name),
      parent_(parent),
      physical_size_in_bytes_(0),
      allocated_objects_count_(0),
      allocated_objects_size_in_bytes_(0) {
  // Dots are not allowed in the name as the underlying base::DictionaryValue
  // would treat them magically and split in sub-nodes, which is not intended.
  DCHECK_EQ(std::string::npos, name.find_first_of('.'));
}

MemoryAllocatorDump::~MemoryAllocatorDump() {
}

void MemoryAllocatorDump::SetExtraAttribute(const std::string& name,
                                            int value) {
  extra_attributes_.SetInteger(name, value);
}

int MemoryAllocatorDump::GetExtraIntegerAttribute(
    const std::string& name) const {
  bool res;
  int value = -1;
  res = extra_attributes_.GetInteger(name, &value);
  DCHECK(res) << "Allocator attribute '" << name << "' not found";
  return value;
}

void MemoryAllocatorDump::AsValueInto(TracedValue* value) const {
  static const char kHexFmt[] = "%" PRIx64;

  value->BeginDictionary(name_.c_str());

  value->SetString("parent", parent_ ? parent_->name_ : "");
  value->SetString("physical_size_in_bytes",
                   StringPrintf(kHexFmt, physical_size_in_bytes_));
  value->SetString("allocated_objects_count",
                   StringPrintf(kHexFmt, allocated_objects_count_));
  value->SetString("allocated_objects_size_in_bytes",
                   StringPrintf(kHexFmt, allocated_objects_size_in_bytes_));

  // Copy all the extra attributes.
  const MemoryDumpProvider* mdp =
      MemoryDumpManager::GetInstance()->dump_provider_currently_active();
  const MemoryAllocatorDeclaredAttributes& extra_attributes_types =
      mdp->allocator_attributes();

  value->BeginDictionary("args");
  for (DictionaryValue::Iterator it(extra_attributes_); !it.IsAtEnd();
       it.Advance()) {
    const std::string& attr_name = it.key();
    const Value& attr_value = it.value();
    value->BeginDictionary(attr_name.c_str());
    value->SetValue("value", attr_value.DeepCopy());

    auto attr_it = extra_attributes_types.find(attr_name);
    DCHECK(attr_it != extra_attributes_types.end())
        << "Allocator attribute " << attr_name
        << " not declared for the dumper " << mdp->GetFriendlyName();

    // TODO(primiano): the "type" should be dumped just once, not repeated on
    // on every event. The ability of doing so depends on crbug.com/466121.
    value->SetString("type", attr_it->second.type);

    value->EndDictionary();  // "arg_name": { "type": "...", "value": "..." }
  }
  value->EndDictionary();  // "args": {}

  value->EndDictionary();  // "allocator name": {}
}

}  // namespace trace_event
}  // namespace base
