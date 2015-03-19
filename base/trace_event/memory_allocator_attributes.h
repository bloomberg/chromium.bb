// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_ALLOCATOR_ATTRIBUTES_H_
#define BASE_TRACE_EVENT_MEMORY_ALLOCATOR_ATTRIBUTES_H_

#include <string>

#include "base/base_export.h"
#include "base/containers/hash_tables.h"

namespace base {
namespace trace_event {

struct BASE_EXPORT MemoryAllocatorDeclaredAttribute {
  std::string name;

  // Refer to src/tools/perf/unit-info.json for the semantic of the type.
  std::string type;
};

using MemoryAllocatorDeclaredAttributes =
    hash_map<std::string, MemoryAllocatorDeclaredAttribute>;

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_ALLOCATOR_ATTRIBUTES_H_
