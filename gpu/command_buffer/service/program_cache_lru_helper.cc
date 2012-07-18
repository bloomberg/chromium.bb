// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_cache_lru_helper.h"

namespace gpu {
namespace gles2 {

ProgramCacheLruHelper::ProgramCacheLruHelper() {}
ProgramCacheLruHelper::~ProgramCacheLruHelper() {}

void ProgramCacheLruHelper::Clear() {
  location_map.clear();
  queue.clear();
}

bool ProgramCacheLruHelper::IsEmpty() {
  return queue.empty();
}

void ProgramCacheLruHelper::KeyUsed(const std::string& key) {
  IteratorMap::iterator location_iterator = location_map.find(key);
  if (location_iterator != location_map.end()) {
    // already exists, erase it
    queue.erase(location_iterator->second);
  }
  queue.push_front(key);
  location_map[key] = queue.begin();
}

const std::string* ProgramCacheLruHelper::PeekKey() {
  if (queue.empty()) {
    return NULL;
  }
  return &queue.back();
}

void ProgramCacheLruHelper::PopKey() {
  if (queue.empty()) {
    return;
  }
  const std::string& last = queue.back();
  location_map.erase(last);
  queue.pop_back();
}

}  // namespace gpu
}  // namespace gles2
