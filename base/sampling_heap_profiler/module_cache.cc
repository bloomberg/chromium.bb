// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/module_cache.h"

#include "base/no_destructor.h"
#include "base/profiler/native_stack_sampler.h"

namespace base {

ModuleCache::ModuleCache() = default;
ModuleCache::~ModuleCache() = default;

const ModuleCache::Module& ModuleCache::GetModuleForAddress(uintptr_t address) {
  static NoDestructor<Module> invalid_module;
  auto it = modules_cache_map_.upper_bound(address);
  if (it != modules_cache_map_.begin()) {
    DCHECK(!modules_cache_map_.empty());
    --it;
    Module& module = it->second;
    if (address < module.base_address + module.size)
      return module;
  }

  auto module = NativeStackSampler::GetModuleForAddress(address);
  if (!module.is_valid)
    return *invalid_module;
  return modules_cache_map_.emplace(module.base_address, std::move(module))
      .first->second;
}

std::vector<const ModuleCache::Module*> ModuleCache::GetModules() const {
  std::vector<const Module*> result;
  result.reserve(modules_cache_map_.size());
  for (const auto& it : modules_cache_map_)
    result.push_back(&it.second);
  return result;
}

}  // namespace base
