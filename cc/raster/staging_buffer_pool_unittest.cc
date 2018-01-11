// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/staging_buffer_pool.h"

#include "base/memory/memory_coordinator_client.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(StagingBufferPoolTest, ShutdownImmediatelyAfterCreation) {
  auto context_provider = TestContextProvider::CreateWorker();
  LayerTreeResourceProvider* resource_provider = nullptr;
  bool use_partial_raster = false;
  int max_staging_buffer_usage_in_bytes = 1024;
  auto task_runner = base::ThreadTaskRunnerHandle::Get();
  // Create a StagingBufferPool and immediately shut it down.
  auto pool = std::make_unique<StagingBufferPool>(
      task_runner.get(), context_provider.get(), resource_provider,
      use_partial_raster, max_staging_buffer_usage_in_bytes);
  pool->Shutdown();
  // Flush the message loop.
  auto flush_message_loop = [] {
    base::RunLoop runloop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  runloop.QuitClosure());
    runloop.Run();
  };

  // Constructing the pool does a post-task to add itself as an observer. So
  // allow for that registration to complete first.
  flush_message_loop();

  // Now, destroy the pool, and trigger a notification from the
  // MemoryCoordinatorClientRegistry.
  pool = nullptr;
  base::MemoryCoordinatorClientRegistry::GetInstance()->PurgeMemory();
  // Allow the callbacks in the observers to run.
  flush_message_loop();
  // No crash.
}

}  // namespace cc
