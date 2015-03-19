// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_ALLOCATOR_DUMP_H_
#define BASE_TRACE_EVENT_MEMORY_ALLOCATOR_DUMP_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/trace_event/memory_allocator_attributes.h"
#include "base/values.h"

namespace base {
namespace trace_event {

class MemoryDumpManager;
class TracedValue;

// Data model for user-land memory allocator dumps.
class BASE_EXPORT MemoryAllocatorDump {
 public:
  MemoryAllocatorDump(const std::string& name, MemoryAllocatorDump* parent);
  ~MemoryAllocatorDump();

  const std::string& name() const { return name_; }
  const MemoryAllocatorDump* parent() const { return parent_; }

  void set_physical_size_in_bytes(uint64 value) {
    physical_size_in_bytes_ = value;
  }
  uint64 physical_size_in_bytes() const { return physical_size_in_bytes_; }

  void set_allocated_objects_count(uint64 value) {
    allocated_objects_count_ = value;
  }
  uint64 allocated_objects_count() const { return allocated_objects_count_; }

  void set_allocated_objects_size_in_bytes(uint64 value) {
    allocated_objects_size_in_bytes_ = value;
  }
  uint64 allocated_objects_size_in_bytes() const {
    return allocated_objects_size_in_bytes_;
  }

  void SetExtraAttribute(const std::string& name, int value);
  int GetExtraIntegerAttribute(const std::string& name) const;

  // Called at trace generation time to populate the TracedValue.
  void AsValueInto(TracedValue* value) const;

 private:
  const std::string name_;
  MemoryAllocatorDump* const parent_;  // Not owned.
  uint64 physical_size_in_bytes_;
  uint64 allocated_objects_count_;
  uint64 allocated_objects_size_in_bytes_;
  DictionaryValue extra_attributes_;

  DISALLOW_COPY_AND_ASSIGN(MemoryAllocatorDump);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_ALLOCATOR_DUMP_H_
