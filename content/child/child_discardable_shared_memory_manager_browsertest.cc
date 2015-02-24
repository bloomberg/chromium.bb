// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "content/child/child_discardable_shared_memory_manager.h"
#include "content/child/child_thread_impl.h"
#include "content/common/host_discardable_shared_memory_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {

class ChildDiscardableSharedMemoryManagerBrowserTest
    : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  static void ReleaseFreeMemory() {
    ChildThreadImpl::current()
        ->discardable_shared_memory_manager()
        ->ReleaseFreeMemory();
  }

  static void AllocateLockedMemory(
      size_t size,
      scoped_ptr<base::DiscardableMemoryShmemChunk>* memory) {
    *memory = ChildThreadImpl::current()
                  ->discardable_shared_memory_manager()
                  ->AllocateLockedDiscardableMemory(size);
  }

  static void LockMemory(base::DiscardableMemoryShmemChunk* memory,
                         bool* result) {
    *result = memory->Lock();
  }

  static void UnlockMemory(base::DiscardableMemoryShmemChunk* memory) {
    memory->Unlock();
  }

  static void FreeMemory(scoped_ptr<base::DiscardableMemoryShmemChunk> memory) {
  }
};

IN_PROC_BROWSER_TEST_F(ChildDiscardableSharedMemoryManagerBrowserTest,
                       DISABLED_LockMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  scoped_ptr<base::DiscardableMemoryShmemChunk> memory;
  PostTaskToInProcessRendererAndWait(base::Bind(
      &ChildDiscardableSharedMemoryManagerBrowserTest::AllocateLockedMemory,
      kSize, &memory));

  ASSERT_TRUE(memory);
  void* addr = memory->Memory();
  ASSERT_NE(nullptr, addr);

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ChildDiscardableSharedMemoryManagerBrowserTest::UnlockMemory,
                 memory.get()));

  // Purge all unlocked memory.
  HostDiscardableSharedMemoryManager::current()->SetMemoryLimit(0);

  bool result = true;
  PostTaskToInProcessRendererAndWait(
      base::Bind(&ChildDiscardableSharedMemoryManagerBrowserTest::LockMemory,
                 memory.get(), &result));

  // Should fail as memory should have been purged.
  EXPECT_FALSE(result);

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ChildDiscardableSharedMemoryManagerBrowserTest::FreeMemory,
                 base::Passed(&memory)));
}

IN_PROC_BROWSER_TEST_F(ChildDiscardableSharedMemoryManagerBrowserTest,
                       DISABLED_AddressSpace) {
  const size_t kLargeSize = 4 * 1024 * 1024;   // 4MiB.
  const size_t kNumberOfInstances = 1024 + 1;  // >4GiB total.

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  scoped_ptr<base::DiscardableMemoryShmemChunk> instances[kNumberOfInstances];
  for (auto& memory : instances) {
    PostTaskToInProcessRendererAndWait(base::Bind(
        &ChildDiscardableSharedMemoryManagerBrowserTest::AllocateLockedMemory,
        kLargeSize, &memory));
    ASSERT_TRUE(memory);
    void* addr = memory->Memory();
    ASSERT_NE(nullptr, addr);
    PostTaskToInProcessRendererAndWait(base::Bind(
        &ChildDiscardableSharedMemoryManagerBrowserTest::UnlockMemory,
        memory.get()));
  }

  for (auto& memory : instances) {
    PostTaskToInProcessRendererAndWait(
        base::Bind(&ChildDiscardableSharedMemoryManagerBrowserTest::FreeMemory,
                   base::Passed(&memory)));
  }
}

}  // content
