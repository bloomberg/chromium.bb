// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_channel_manager_delegate.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace base {
class TestSimpleTaskRunner;
}  // namespace base

namespace IPC {
class TestSink;
}  // namespace IPC

namespace content {

class SyncPointManager;

class TestGpuChannelManagerDelegate : public GpuChannelManagerDelegate {
 public:
  TestGpuChannelManagerDelegate();
  ~TestGpuChannelManagerDelegate() override;

 private:
  // GpuChannelManagerDelegate implementation:
  void SetActiveURL(const GURL& url) override;
  void AddSubscription(int32_t client_id, unsigned int target) override;
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void GpuMemoryUmaStats(const gpu::GPUMemoryUmaStats& params) override;
  void RemoveSubscription(int32_t client_id, unsigned int target) override;
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;
#if defined(OS_MACOSX)
  void SendAcceleratedSurfaceBuffersSwapped(
      const AcceleratedSurfaceBuffersSwappedParams& params) override;
#endif
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestGpuChannelManagerDelegate);
};

class TestGpuChannelManager : public GpuChannelManager {
 public:
  TestGpuChannelManager(const gpu::GpuPreferences& gpu_preferences,
                        GpuChannelManagerDelegate* delegate,
                        base::SingleThreadTaskRunner* task_runner,
                        base::SingleThreadTaskRunner* io_task_runner,
                        gpu::SyncPointManager* sync_point_manager,
                        GpuMemoryBufferFactory* gpu_memory_buffer_factory);
  ~TestGpuChannelManager() override;

 protected:
  scoped_ptr<GpuChannel> CreateGpuChannel(
      int client_id,
      uint64_t client_tracing_id,
      bool preempts,
      bool allow_view_command_buffers,
      bool allow_real_time_streams) override;
};

class TestGpuChannel : public GpuChannel {
 public:
  TestGpuChannel(GpuChannelManager* gpu_channel_manager,
                 gpu::SyncPointManager* sync_point_manager,
                 gfx::GLShareGroup* share_group,
                 gpu::gles2::MailboxManager* mailbox_manager,
                 gpu::PreemptionFlag* preempting_flag,
                 gpu::PreemptionFlag* preempted_flag,
                 base::SingleThreadTaskRunner* task_runner,
                 base::SingleThreadTaskRunner* io_task_runner,
                 int client_id,
                 uint64_t client_tracing_id,
                 bool allow_view_command_buffers,
                 bool allow_real_time_streams);
  ~TestGpuChannel() override;

  IPC::TestSink* sink() { return &sink_; }
  base::ProcessId GetClientPID() const override;

  IPC::ChannelHandle Init(base::WaitableEvent* shutdown_event) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

 private:
  IPC::TestSink sink_;
};

class GpuChannelTestCommon : public testing::Test {
 public:
  GpuChannelTestCommon();
  ~GpuChannelTestCommon() override;

 protected:
  GpuChannelManager* channel_manager() { return channel_manager_.get(); }
  TestGpuChannelManagerDelegate* channel_manager_delegate() {
    return channel_manager_delegate_.get();
  }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  gpu::GpuPreferences gpu_preferences_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> io_task_runner_;
  scoped_ptr<gpu::SyncPointManager> sync_point_manager_;
  scoped_ptr<TestGpuChannelManagerDelegate> channel_manager_delegate_;
  scoped_ptr<GpuChannelManager> channel_manager_;
};

}  // namespace content
