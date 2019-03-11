// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/sampling_heap_profiler/module_cache.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

int AFunctionForTest() {
  return 42;
}

// Provides a module that is guaranteed to be isolated from (and non-contiguous
// with) any other module, by placing the module in the middle of a block of
// heap memory.
class IsolatedModule : public ModuleCache::Module {
 public:
  IsolatedModule() : memory_region_(new char[kRegionSize]) {}

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override {
    // Place the module in the middle of the region.
    return reinterpret_cast<uintptr_t>(&memory_region_[kRegionSize / 4]);
  }

  std::string GetId() const override { return ""; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return kRegionSize / 2; }

 private:
  static const int kRegionSize = 100;

  std::unique_ptr<char[]> memory_region_;
};

#if defined(OS_MACOSX) && !defined(OS_IOS) || defined(OS_WIN)
#define MAYBE_TEST(TestSuite, TestName) TEST(TestSuite, TestName)
#else
#define MAYBE_TEST(TestSuite, TestName) TEST(TestSuite, DISABLED_##TestName)
#endif

// Checks that ModuleCache returns the same module instance for
// addresses within the module.
MAYBE_TEST(ModuleCacheTest, LookupCodeAddresses) {
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

MAYBE_TEST(ModuleCacheTest, LookupRange) {
  ModuleCache cache;
  auto to_inject = std::make_unique<IsolatedModule>();
  const ModuleCache::Module* module = to_inject.get();
  cache.InjectModuleForTesting(std::move(to_inject));

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() - 1));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress()));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress() +
                                              module->GetSize() - 1));
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() +
                                               module->GetSize()));
}

MAYBE_TEST(ModuleCacheTest, ModulesList) {
  ModuleCache cache;
  uintptr_t ptr = reinterpret_cast<uintptr_t>(&AFunctionForTest);
  const ModuleCache::Module* module = cache.GetModuleForAddress(ptr);
  EXPECT_NE(nullptr, module);
  EXPECT_EQ(1u, cache.GetModules().size());
  EXPECT_EQ(module, cache.GetModules().front());
}

MAYBE_TEST(ModuleCacheTest, InvalidModule) {
  ModuleCache cache;
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));
}

}  // namespace
}  // namespace base
