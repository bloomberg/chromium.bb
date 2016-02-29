// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/gpu/establish_channel_params.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager_delegate.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/image_transport_surface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_share_group.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/buffer_presented_params_mac.h"
#endif

namespace content {

namespace {
#if defined(OS_ANDROID)
// Amount of time we expect the GPU to stay powered up without being used.
const int kMaxGpuIdleTimeMs = 40;
// Maximum amount of time we keep pinging the GPU waiting for the client to
// draw.
const int kMaxKeepAliveTimeMs = 200;
#endif

}

GpuChannelManager::GpuChannelManager(
    const gpu::GpuPreferences& gpu_preferences,
    GpuChannelManagerDelegate* delegate,
    GpuWatchdog* watchdog,
    base::SingleThreadTaskRunner* task_runner,
    base::SingleThreadTaskRunner* io_task_runner,
    base::WaitableEvent* shutdown_event,
    gpu::SyncPointManager* sync_point_manager,
    GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      gpu_preferences_(gpu_preferences),
      delegate_(delegate),
      watchdog_(watchdog),
      shutdown_event_(shutdown_event),
      share_group_(new gfx::GLShareGroup),
      mailbox_manager_(gpu::gles2::MailboxManager::Create(gpu_preferences)),
      gpu_memory_manager_(this),
      sync_point_manager_(sync_point_manager),
      sync_point_client_waiter_(
          sync_point_manager->CreateSyncPointClientWaiter()),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      weak_factory_(this) {
  DCHECK(task_runner);
  DCHECK(io_task_runner);
  if (gpu_preferences_.ui_prioritize_in_gpu_process)
    preemption_flag_ = new gpu::PreemptionFlag;
}

GpuChannelManager::~GpuChannelManager() {
  // Destroy channels before anything else because of dependencies.
  gpu_channels_.clear();
  if (default_offscreen_surface_.get()) {
    default_offscreen_surface_->Destroy();
    default_offscreen_surface_ = NULL;
  }
}

gpu::gles2::ProgramCache* GpuChannelManager::program_cache() {
  if (!program_cache_.get() &&
      (gfx::g_driver_gl.ext.b_GL_ARB_get_program_binary ||
       gfx::g_driver_gl.ext.b_GL_OES_get_program_binary) &&
      !gpu_preferences_.disable_gpu_program_cache) {
    program_cache_.reset(new gpu::gles2::MemoryProgramCache(
          gpu_preferences_.gpu_program_cache_size,
          gpu_preferences_.disable_gpu_shader_disk_cache));
  }
  return program_cache_.get();
}

gpu::gles2::ShaderTranslatorCache*
GpuChannelManager::shader_translator_cache() {
  if (!shader_translator_cache_.get()) {
    shader_translator_cache_ =
        new gpu::gles2::ShaderTranslatorCache(gpu_preferences_);
  }
  return shader_translator_cache_.get();
}

gpu::gles2::FramebufferCompletenessCache*
GpuChannelManager::framebuffer_completeness_cache() {
  if (!framebuffer_completeness_cache_.get())
    framebuffer_completeness_cache_ =
        new gpu::gles2::FramebufferCompletenessCache;
  return framebuffer_completeness_cache_.get();
}

void GpuChannelManager::RemoveChannel(int client_id) {
  delegate_->DidDestroyChannel(client_id);
  gpu_channels_.erase(client_id);
}

#if defined(OS_MACOSX)
void GpuChannelManager::AddImageTransportSurface(
    int32_t surface_id,
    ImageTransportHelper* image_transport_helper) {
  image_transport_map_.AddWithID(image_transport_helper, surface_id);
}

void GpuChannelManager::RemoveImageTransportSurface(int32_t surface_id) {
  image_transport_map_.Remove(surface_id);
}

void GpuChannelManager::BufferPresented(const BufferPresentedParams& params) {
  ImageTransportHelper* helper = image_transport_map_.Lookup(params.surface_id);
  if (helper)
    helper->BufferPresented(params);
}
#endif

GpuChannel* GpuChannelManager::LookupChannel(int32_t client_id) const {
  const auto& it = gpu_channels_.find(client_id);
  return it != gpu_channels_.end() ? it->second : nullptr;
}

scoped_ptr<GpuChannel> GpuChannelManager::CreateGpuChannel(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams) {
  return make_scoped_ptr(
      new GpuChannel(this, sync_point_manager(), watchdog_, share_group(),
                     mailbox_manager(), preempts ? preemption_flag() : nullptr,
                     preempts ? nullptr : preemption_flag(), task_runner_.get(),
                     io_task_runner_.get(), client_id, client_tracing_id,
                     allow_view_command_buffers, allow_real_time_streams));
}

void GpuChannelManager::EstablishChannel(const EstablishChannelParams& params) {
  scoped_ptr<GpuChannel> channel(CreateGpuChannel(
      params.client_id, params.client_tracing_id, params.preempts,
      params.allow_view_command_buffers, params.allow_real_time_streams));
  IPC::ChannelHandle channel_handle = channel->Init(shutdown_event_);

  gpu_channels_.set(params.client_id, std::move(channel));

  delegate_->ChannelEstablished(channel_handle);
}

void GpuChannelManager::CloseChannel(const IPC::ChannelHandle& channel_handle) {
  for (auto it = gpu_channels_.begin(); it != gpu_channels_.end(); ++it) {
    if (it->second->channel_id() == channel_handle.name) {
      gpu_channels_.erase(it);
      return;
    }
  }
}

void GpuChannelManager::InternalDestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannelManager::InternalDestroyGpuMemoryBufferOnIO,
                 base::Unretained(this), id, client_id));
}

void GpuChannelManager::InternalDestroyGpuMemoryBufferOnIO(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  gpu_memory_buffer_factory_->DestroyGpuMemoryBuffer(id, client_id);
}

void GpuChannelManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    scoped_refptr<gpu::SyncPointClientState> release_state =
        sync_point_manager()->GetSyncPointClientState(
            sync_token.namespace_id(), sync_token.command_buffer_id());
    if (release_state) {
      sync_point_client_waiter_->WaitOutOfOrder(
          release_state.get(), sync_token.release_count(),
          base::Bind(&GpuChannelManager::InternalDestroyGpuMemoryBuffer,
                     base::Unretained(this), id, client_id));
      return;
    }
  }

  // No sync token or invalid sync token, destroy immediately.
  InternalDestroyGpuMemoryBuffer(id, client_id);
}

void GpuChannelManager::UpdateValueState(int client_id,
                                         unsigned int target,
                                         const gpu::ValueState& state) {
  // Only pass updated state to the channel corresponding to the
  // render_widget_host where the event originated.
  auto it = gpu_channels_.find(client_id);
  if (it != gpu_channels_.end())
    it->second->HandleUpdateValueState(target, state);
}

void GpuChannelManager::PopulateShaderCache(const std::string& program_proto) {
  if (program_cache())
    program_cache()->LoadProgram(program_proto);
}

uint32_t GpuChannelManager::GetUnprocessedOrderNum() const {
  uint32_t unprocessed_order_num = 0;
  for (auto& kv : gpu_channels_) {
    unprocessed_order_num =
        std::max(unprocessed_order_num, kv.second->GetUnprocessedOrderNum());
  }
  return unprocessed_order_num;
}

uint32_t GpuChannelManager::GetProcessedOrderNum() const {
  uint32_t processed_order_num = 0;
  for (auto& kv : gpu_channels_) {
    processed_order_num =
        std::max(processed_order_num, kv.second->GetProcessedOrderNum());
  }
  return processed_order_num;
}

void GpuChannelManager::LoseAllContexts() {
  for (auto& kv : gpu_channels_) {
    kv.second->MarkAllContextsLost();
  }
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&GpuChannelManager::DestroyAllChannels,
                                    weak_factory_.GetWeakPtr()));
}

void GpuChannelManager::DestroyAllChannels() {
  gpu_channels_.clear();
}

gfx::GLSurface* GpuChannelManager::GetDefaultOffscreenSurface() {
  if (!default_offscreen_surface_.get()) {
    default_offscreen_surface_ =
        gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size());
  }
  return default_offscreen_surface_.get();
}

#if defined(OS_ANDROID)
void GpuChannelManager::DidAccessGpu() {
  last_gpu_access_time_ = base::TimeTicks::Now();
}

void GpuChannelManager::WakeUpGpu() {
  begin_wake_up_time_ = base::TimeTicks::Now();
  ScheduleWakeUpGpu();
}

void GpuChannelManager::ScheduleWakeUpGpu() {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT2("gpu", "GpuChannelManager::ScheduleWakeUp",
               "idle_time", (now - last_gpu_access_time_).InMilliseconds(),
               "keep_awake_time", (now - begin_wake_up_time_).InMilliseconds());
  if (now - last_gpu_access_time_ <
      base::TimeDelta::FromMilliseconds(kMaxGpuIdleTimeMs))
    return;
  if (now - begin_wake_up_time_ >
      base::TimeDelta::FromMilliseconds(kMaxKeepAliveTimeMs))
    return;

  DoWakeUpGpu();

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&GpuChannelManager::ScheduleWakeUpGpu,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kMaxGpuIdleTimeMs));
}

void GpuChannelManager::DoWakeUpGpu() {
  const GpuCommandBufferStub* stub = nullptr;
  for (const auto& kv : gpu_channels_) {
    const GpuChannel* channel = kv.second;
    stub = channel->GetOneStub();
    if (stub) {
      DCHECK(stub->decoder());
      break;
    }
  }
  if (!stub || !stub->decoder()->MakeCurrent())
    return;
  glFinish();
  DidAccessGpu();
}
#endif

}  // namespace content
