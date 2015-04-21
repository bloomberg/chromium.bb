// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_attributes_type_info.h"

#include "base/lazy_instance.h"
#include "base/logging.h"

namespace base {
namespace trace_event {

namespace {
base::LazyInstance<const std::string> kNotFound = LAZY_INSTANCE_INITIALIZER;

std::string GetKey(const std::string& allocator_name,
                   const std::string& attribute_name) {
  return allocator_name + "/" + attribute_name;
}
}  // namespace

MemoryAllocatorAttributesTypeInfo::MemoryAllocatorAttributesTypeInfo() {
}

MemoryAllocatorAttributesTypeInfo::~MemoryAllocatorAttributesTypeInfo() {
}

const std::string& MemoryAllocatorAttributesTypeInfo::Get(
    const std::string& allocator_name,
    const std::string& attribute_name) const {
  auto it = type_info_map_.find(GetKey(allocator_name, attribute_name));
  if (it == type_info_map_.end())
    return kNotFound.Get();
  return it->second;
}

void MemoryAllocatorAttributesTypeInfo::Set(const std::string& allocator_name,
                                            const std::string& attribute_name,
                                            const std::string& attribute_type) {
  std::string key = GetKey(allocator_name, attribute_name);
  DCHECK_EQ(0u, type_info_map_.count(key));
  type_info_map_[key] = attribute_type;
}

bool MemoryAllocatorAttributesTypeInfo::Exists(
    const std::string& allocator_name,
    const std::string& attribute_name) const {
  return type_info_map_.count(GetKey(allocator_name, attribute_name)) == 1;
}

void MemoryAllocatorAttributesTypeInfo::Update(
    const MemoryAllocatorAttributesTypeInfo& other) {
  for (auto it = other.type_info_map_.begin();
       it != other.type_info_map_.end(); ++it) {
    bool no_duplicates = type_info_map_.insert(*it).second;
    DCHECK(no_duplicates) << "Duplicated allocator attribute " << it->first;
  }
}

}  // namespace trace_event
}  // namespace base
