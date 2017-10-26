// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class TestSimpleTaskRunner;
}  // namespace base

namespace IPC {
class Message;
}  // namespace IPC

namespace gpu {
class GpuChannel;
class GpuChannelManager;
class Scheduler;
class SyncPointManager;
class TestGpuChannelManagerDelegate;

class GpuChannelTestCommon : public testing::Test {
 public:
  GpuChannelTestCommon();
  ~GpuChannelTestCommon() override;

 protected:
  GpuChannelManager* channel_manager() { return channel_manager_.get(); }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

  GpuChannel* CreateChannel(int32_t client_id, bool is_gpu_host);

  void HandleMessage(GpuChannel* channel, IPC::Message* msg);

  base::SharedMemoryHandle GetSharedHandle();

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> io_task_runner_;
  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<TestGpuChannelManagerDelegate> channel_manager_delegate_;
  std::unique_ptr<GpuChannelManager> channel_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelTestCommon);
};

}  // namespace gpu
