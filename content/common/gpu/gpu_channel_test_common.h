// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class TestSimpleTaskRunner;
}  // namespace base

namespace IPC {
class TestSink;
}  // namespace IPC

namespace content {

class SyncPointManager;

class TestGpuChannelManager : public GpuChannelManager {
 public:
  TestGpuChannelManager(IPC::TestSink* sink,
                        base::SingleThreadTaskRunner* task_runner,
                        base::SingleThreadTaskRunner* io_task_runner,
                        gpu::SyncPointManager* sync_point_manager,
                        GpuMemoryBufferFactory* gpu_memory_buffer_factory);
  ~TestGpuChannelManager() override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

 protected:
  scoped_ptr<GpuChannel> CreateGpuChannel(
      int client_id,
      uint64_t client_tracing_id,
      bool preempts,
      bool allow_future_sync_points,
      bool allow_real_time_streams) override;

 private:
  IPC::TestSink* const sink_;
};

class TestGpuChannel : public GpuChannel {
 public:
  TestGpuChannel(IPC::TestSink* sink,
                 GpuChannelManager* gpu_channel_manager,
                 gpu::SyncPointManager* sync_point_manager,
                 gfx::GLShareGroup* share_group,
                 gpu::gles2::MailboxManager* mailbox_manager,
                 gpu::PreemptionFlag* preempting_flag,
                 base::SingleThreadTaskRunner* task_runner,
                 base::SingleThreadTaskRunner* io_task_runner,
                 int client_id,
                 uint64_t client_tracing_id,
                 bool allow_future_sync_points,
                 bool allow_real_time_streams);
  ~TestGpuChannel() override;

  base::ProcessId GetClientPID() const override;

  IPC::ChannelHandle Init(base::WaitableEvent* shutdown_event) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

 private:
  IPC::TestSink* const sink_;
};

class GpuChannelTestCommon : public testing::Test {
 public:
  GpuChannelTestCommon();
  ~GpuChannelTestCommon() override;

 protected:
  IPC::TestSink* sink() { return sink_.get(); }
  GpuChannelManager* channel_manager() { return channel_manager_.get(); }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  scoped_ptr<IPC::TestSink> sink_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> io_task_runner_;
  scoped_ptr<gpu::SyncPointManager> sync_point_manager_;
  scoped_ptr<GpuChannelManager> channel_manager_;
};

}  // namespace content
