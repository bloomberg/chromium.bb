// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/module_cache.h"

#include <utility>

namespace base {

ModuleCache::Module::Module(uintptr_t base_address,
                            const std::string& id,
                            const FilePath& filename,
                            size_t size)
    : base_address_(base_address), id_(id), filename_(filename), size_(size) {}

ModuleCache::Module::~Module() = default;

uintptr_t ModuleCache::Module::GetBaseAddress() const {
  return base_address_;
}

std::string ModuleCache::Module::GetId() const {
  return id_;
}

FilePath ModuleCache::Module::GetFilename() const {
  return filename_;
}

size_t ModuleCache::Module::GetSize() const {
  return size_;
}

ModuleCache::ModuleCache() = default;
ModuleCache::~ModuleCache() = default;

const ModuleCache::Module* ModuleCache::GetModuleForAddress(uintptr_t address) {
  auto it = modules_cache_map_.upper_bound(address);
  if (it != modules_cache_map_.begin()) {
    DCHECK(!modules_cache_map_.empty());
    --it;
    const Module* module = it->second.get();
    if (address < module->GetBaseAddress() + module->GetSize())
      return module;
  }

  std::unique_ptr<Module> module = CreateModuleForAddress(address);
  if (!module)
    return nullptr;
  return modules_cache_map_.emplace(module->GetBaseAddress(), std::move(module))
      .first->second.get();
}

std::vector<const ModuleCache::Module*> ModuleCache::GetModules() const {
  std::vector<const Module*> result;
  result.reserve(modules_cache_map_.size());
  for (const auto& it : modules_cache_map_)
    result.push_back(it.second.get());
  return result;
}

}  // namespace base
