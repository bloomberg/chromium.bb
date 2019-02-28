// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/module_cache.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class ModuleCacheTest : public ::testing::Test {};

int AFunctionForTest() {
  return 42;
}

// Checks that ModuleCache returns the same module instance for
// addresses within the module.
#if defined(OS_MACOSX) && !defined(OS_IOS) || defined(OS_WIN)
#define MAYBE_ModuleCache ModuleCache
#define MAYBE_ModulesList ModulesList
#else
#define MAYBE_ModuleCache DISABLED_ModuleCache
#define MAYBE_ModulesList DISABLED_ModulesList
#endif
TEST_F(ModuleCacheTest, MAYBE_ModuleCache) {
  uintptr_t ptr1 = reinterpret_cast<uintptr_t>(&AFunctionForTest);
  uintptr_t ptr2 = ptr1 + 1;
  ModuleCache cache;
  const ModuleCache::Module* module1 = cache.GetModuleForAddress(ptr1);
  const ModuleCache::Module* module2 = cache.GetModuleForAddress(ptr2);
  EXPECT_EQ(module1, module2);
  EXPECT_NE(nullptr, module1);
  EXPECT_GT(module1->GetSize(), 0u);
  EXPECT_LE(module1->GetBaseAddress(), ptr1);
  EXPECT_GT(module1->GetBaseAddress() + module1->GetSize(), ptr2);
}

TEST_F(ModuleCacheTest, MAYBE_ModulesList) {
  ModuleCache cache;
  uintptr_t ptr = reinterpret_cast<uintptr_t>(&AFunctionForTest);
  const ModuleCache::Module* module = cache.GetModuleForAddress(ptr);
  EXPECT_NE(nullptr, module);
  EXPECT_EQ(1u, cache.GetModules().size());
  EXPECT_EQ(module, cache.GetModules().front());
}

TEST_F(ModuleCacheTest, InvalidModule) {
  ModuleCache cache;
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));
}

}  // namespace
}  // namespace base
