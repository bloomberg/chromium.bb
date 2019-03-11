// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/module_cache.h"

#include <utility>

namespace base {

ModuleCache::ModuleCache() = default;
ModuleCache::~ModuleCache() = default;

const ModuleCache::Module* ModuleCache::GetModuleForAddress(uintptr_t address) {
  auto it = std::find_if(modules_.begin(), modules_.end(),
                         [address](const std::unique_ptr<Module>& module) {
                           return address >= module->GetBaseAddress() &&
                                  address < module->GetBaseAddress() +
                                                module->GetSize();
                         });
  if (it != modules_.end())
    return it->get();

  std::unique_ptr<Module> module = CreateModuleForAddress(address);
  if (!module)
    return nullptr;
  modules_.push_back(std::move(module));
  return modules_.back().get();
}

std::vector<const ModuleCache::Module*> ModuleCache::GetModules() const {
  std::vector<const Module*> result;
  result.reserve(modules_.size());
  for (const std::unique_ptr<Module>& module : modules_)
    result.push_back(module.get());
  return result;
}

void ModuleCache::InjectModuleForTesting(std::unique_ptr<Module> module) {
  modules_.push_back(std::move(module));
}

}  // namespace base
