// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_ALLOCATOR_ATTRIBUTES_TYPE_INFO_H_
#define BASE_TRACE_EVENT_MEMORY_ALLOCATOR_ATTRIBUTES_TYPE_INFO_H_

#include <string>

#include "base/base_export.h"
#include "base/containers/hash_tables.h"

namespace base {
namespace trace_event {

// A dictionary of "allocator_name/attribute_name" -> "attribute_type" which
// supports merging and enforces duplicate checking.
class BASE_EXPORT MemoryAllocatorAttributesTypeInfo {
 public:
  MemoryAllocatorAttributesTypeInfo();
  ~MemoryAllocatorAttributesTypeInfo();

  // Returns the attribute type, or an empty string if not found.
  const std::string& Get(const std::string& allocator_name,
                         const std::string& attribute_name) const;

  // Refer to tools/perf/unit-info.json for the semantics of |attribute_type|.
  void Set(const std::string& allocator_name,
           const std::string& attribute_name,
           const std::string& attribute_type);

  // Checks whether a given {allocator_name, attribute_name} declaration exists.
  bool Exists(const std::string& allocator_name,
              const std::string& attribute_name) const;

  // Merges the attribute types declared in |other| into this.
  void Update(const MemoryAllocatorAttributesTypeInfo& other);

 private:
  // "allocator_name/attribute_name" -> attribute_type.
  hash_map<std::string, std::string> type_info_map_;

  DISALLOW_COPY_AND_ASSIGN(MemoryAllocatorAttributesTypeInfo);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_ALLOCATOR_ATTRIBUTES_TYPE_INFO_H_
