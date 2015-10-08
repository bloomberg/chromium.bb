// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel_test_common.h"

#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "ipc/ipc_test_sink.h"

namespace content {

TestGpuChannelManager::TestGpuChannelManager(
    IPC::TestSink* sink,
    base::SingleThreadTaskRunner* task_runner,
    base::SingleThreadTaskRunner* io_task_runner,
    gpu::SyncPointManager* sync_point_manager,
    GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : GpuChannelManager(nullptr,
                        nullptr,
                        task_runner,
                        io_task_runner,
                        nullptr,
                        sync_point_manager,
                        gpu_memory_buffer_factory),
      sink_(sink) {}

TestGpuChannelManager::~TestGpuChannelManager() {
  // Clear gpu channels here so that any IPC messages sent are handled using the
  // overridden Send method.
  gpu_channels_.clear();
}

bool TestGpuChannelManager::Send(IPC::Message* msg) {
  return sink_->Send(msg);
}

scoped_ptr<GpuChannel> TestGpuChannelManager::CreateGpuChannel(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_future_sync_points,
    bool allow_real_time_streams) {
  return make_scoped_ptr(new TestGpuChannel(
      sink_, this, sync_point_manager(), share_group(), mailbox_manager(),
      preempts ? preemption_flag() : nullptr, task_runner_.get(),
      io_task_runner_.get(), client_id, client_tracing_id,
      allow_future_sync_points, allow_real_time_streams));
}

TestGpuChannel::TestGpuChannel(IPC::TestSink* sink,
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
                               bool allow_real_time_streams)
    : GpuChannel(gpu_channel_manager,
                 sync_point_manager,
                 nullptr,
                 share_group,
                 mailbox_manager,
                 preempting_flag,
                 task_runner,
                 io_task_runner,
                 client_id,
                 client_tracing_id,
                 allow_future_sync_points,
                 allow_real_time_streams),
      sink_(sink) {}

TestGpuChannel::~TestGpuChannel() {
  // Call stubs here so that any IPC messages sent are handled using the
  // overridden Send method.
  stubs_.clear();
}

base::ProcessId TestGpuChannel::GetClientPID() const {
  return base::kNullProcessId;
}

IPC::ChannelHandle TestGpuChannel::Init(base::WaitableEvent* shutdown_event) {
  filter_->OnFilterAdded(sink_);
  return IPC::ChannelHandle(channel_id());
}

bool TestGpuChannel::Send(IPC::Message* msg) {
  DCHECK(!msg->is_sync());
  return sink_->Send(msg);
}

// TODO(sunnyps): Use a mock memory buffer factory when necessary.
GpuChannelTestCommon::GpuChannelTestCommon()
    : sink_(new IPC::TestSink),
      task_runner_(new base::TestSimpleTaskRunner),
      io_task_runner_(new base::TestSimpleTaskRunner),
      sync_point_manager_(new gpu::SyncPointManager(false)),
      channel_manager_(new TestGpuChannelManager(sink_.get(),
                                                 task_runner_.get(),
                                                 io_task_runner_.get(),
                                                 sync_point_manager_.get(),
                                                 nullptr)) {}

GpuChannelTestCommon::~GpuChannelTestCommon() {}

}  // namespace content
