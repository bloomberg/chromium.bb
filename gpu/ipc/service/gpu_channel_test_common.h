// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_TEST_COMMON_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_TEST_COMMON_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class TestSimpleTaskRunner;
namespace trace_event {
class MemoryDumpManager;
}  // namespace trace_event
}  // namespace base

namespace IPC {
class Message;
}  // namespace IPC

namespace gpu {
class GpuChannel;
class GpuChannelManager;
class Scheduler;
class SyncPointManager;
class SharedImageManager;
class TestGpuChannelManagerDelegate;

class GpuChannelTestCommon : public testing::Test {
 public:
  explicit GpuChannelTestCommon(bool use_stub_bindings);
  // Constructor which allows a custom set of GPU driver bug workarounds.
  GpuChannelTestCommon(std::vector<int32_t> enabled_workarounds,
                       bool use_stub_bindings);
  ~GpuChannelTestCommon() override;

 protected:
  Scheduler* scheduler() const { return scheduler_.get(); }
  GpuChannelManager* channel_manager() const { return channel_manager_.get(); }
  base::TestSimpleTaskRunner* task_runner() const { return task_runner_.get(); }
  base::TestSimpleTaskRunner* io_task_runner() const {
    return io_task_runner_.get();
  }

  GpuChannel* CreateChannel(int32_t client_id, bool is_gpu_host);

  void HandleMessage(GpuChannel* channel, IPC::Message* msg);

  base::UnsafeSharedMemoryRegion GetSharedMemoryRegion();

 private:
  std::unique_ptr<base::trace_event::MemoryDumpManager> memory_dump_manager_;
  IPC::TestSink sink_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> io_task_runner_;
  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<TestGpuChannelManagerDelegate> channel_manager_delegate_;
  std::unique_ptr<GpuChannelManager> channel_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelTestCommon);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_TEST_COMMON_H_
