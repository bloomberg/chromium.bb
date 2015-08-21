// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/message_router.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "ipc/message_filter.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_share_group.h"

namespace content {

GpuChannelManager::GpuChannelManager(
    IPC::SyncChannel* channel,
    GpuWatchdog* watchdog,
    base::SingleThreadTaskRunner* task_runner,
    base::SingleThreadTaskRunner* io_task_runner,
    base::WaitableEvent* shutdown_event,
    IPC::AttachmentBroker* broker,
    gpu::SyncPointManager* sync_point_manager,
    GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      channel_(channel),
      watchdog_(watchdog),
      shutdown_event_(shutdown_event),
      gpu_memory_manager_(
          this,
          GpuMemoryManager::kDefaultMaxSurfacesWithFrontbufferSoftLimit),
      sync_point_manager_(sync_point_manager),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      attachment_broker_(broker),
      weak_factory_(this) {
  DCHECK(task_runner);
  DCHECK(io_task_runner);
}

GpuChannelManager::~GpuChannelManager() {
  if (default_offscreen_surface_.get()) {
    default_offscreen_surface_->Destroy();
    default_offscreen_surface_ = NULL;
  }
}

gpu::gles2::ProgramCache* GpuChannelManager::program_cache() {
  if (!program_cache_.get() &&
      (gfx::g_driver_gl.ext.b_GL_ARB_get_program_binary ||
       gfx::g_driver_gl.ext.b_GL_OES_get_program_binary) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuProgramCache)) {
    program_cache_.reset(new gpu::gles2::MemoryProgramCache());
  }
  return program_cache_.get();
}

gpu::gles2::ShaderTranslatorCache*
GpuChannelManager::shader_translator_cache() {
  if (!shader_translator_cache_.get())
    shader_translator_cache_ = new gpu::gles2::ShaderTranslatorCache;
  return shader_translator_cache_.get();
}

void GpuChannelManager::RemoveChannel(int client_id) {
  Send(new GpuHostMsg_DestroyChannel(client_id));
  gpu_channels_.erase(client_id);
}

int GpuChannelManager::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannelManager::AddRoute(int32 routing_id, IPC::Listener* listener) {
  router_.AddRoute(routing_id, listener);
}

void GpuChannelManager::RemoveRoute(int32 routing_id) {
  router_.RemoveRoute(routing_id);
}

GpuChannel* GpuChannelManager::LookupChannel(int32 client_id) {
  const auto& it = gpu_channels_.find(client_id);
  return it != gpu_channels_.end() ? it->second : nullptr;
}

bool GpuChannelManager::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannelManager, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel, OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateViewCommandBuffer,
                        OnCreateViewCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_DestroyGpuMemoryBuffer, OnDestroyGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_LoadedShader, OnLoadedShader)
    IPC_MESSAGE_HANDLER(GpuMsg_UpdateValueState, OnUpdateValueState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool GpuChannelManager::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return router_.RouteMessage(msg);
}

bool GpuChannelManager::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

scoped_ptr<GpuChannel> GpuChannelManager::CreateGpuChannel(
    gfx::GLShareGroup* share_group,
    gpu::gles2::MailboxManager* mailbox_manager,
    int client_id,
    uint64_t client_tracing_id,
    bool allow_future_sync_points) {
  return make_scoped_ptr(
      new GpuChannel(this, watchdog_, share_group, mailbox_manager,
                     task_runner_.get(), io_task_runner_.get(), client_id,
                     client_tracing_id, false, allow_future_sync_points));
}

void GpuChannelManager::OnEstablishChannel(int client_id,
                                           uint64_t client_tracing_id,
                                           bool share_context,
                                           bool allow_future_sync_points) {
  gfx::GLShareGroup* share_group = nullptr;
  gpu::gles2::MailboxManager* mailbox_manager = nullptr;
  if (share_context) {
    if (!share_group_.get()) {
      share_group_ = new gfx::GLShareGroup;
      DCHECK(!mailbox_manager_.get());
      mailbox_manager_ = gpu::gles2::MailboxManager::Create();
    }
    share_group = share_group_.get();
    mailbox_manager = mailbox_manager_.get();
  }

  scoped_ptr<GpuChannel> channel =
      CreateGpuChannel(share_group, mailbox_manager, client_id,
                       client_tracing_id, allow_future_sync_points);
  IPC::ChannelHandle channel_handle =
      channel->Init(shutdown_event_, attachment_broker_);

  gpu_channels_.set(client_id, channel.Pass());

  Send(new GpuHostMsg_ChannelEstablished(channel_handle));
}

void GpuChannelManager::OnCloseChannel(
    const IPC::ChannelHandle& channel_handle) {
  for (auto it = gpu_channels_.begin(); it != gpu_channels_.end(); ++it) {
    if (it->second->channel_id() == channel_handle.name) {
      gpu_channels_.erase(it);
      return;
    }
  }
}

void GpuChannelManager::OnCreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    int32 surface_id,
    int32 client_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id) {
  DCHECK(surface_id);
  CreateCommandBufferResult result = CREATE_COMMAND_BUFFER_FAILED;

  auto it = gpu_channels_.find(client_id);
  if (it != gpu_channels_.end()) {
    result = it->second->CreateViewCommandBuffer(window, surface_id,
                                                 init_params, route_id);
  }

  Send(new GpuHostMsg_CommandBufferCreated(result));
}

void GpuChannelManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelManager::DestroyGpuMemoryBufferOnIO,
                            base::Unretained(this), id, client_id));
}

void GpuChannelManager::DestroyGpuMemoryBufferOnIO(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  gpu_memory_buffer_factory_->DestroyGpuMemoryBuffer(id, client_id);
}

void GpuChannelManager::OnDestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    int32 sync_point) {
  if (!sync_point) {
    DestroyGpuMemoryBuffer(id, client_id);
  } else {
    sync_point_manager()->AddSyncPointCallback(
        sync_point,
        base::Bind(&GpuChannelManager::DestroyGpuMemoryBuffer,
                   base::Unretained(this),
                   id,
                   client_id));
  }
}

void GpuChannelManager::OnUpdateValueState(
    int client_id, unsigned int target, const gpu::ValueState& state) {
  // Only pass updated state to the channel corresponding to the
  // render_widget_host where the event originated.
  auto it = gpu_channels_.find(client_id);
  if (it != gpu_channels_.end())
    it->second->HandleUpdateValueState(target, state);
}

void GpuChannelManager::OnLoadedShader(std::string program_proto) {
  if (program_cache())
    program_cache()->LoadProgram(program_proto);
}

bool GpuChannelManager::HandleMessagesScheduled() {
  for (auto& kv : gpu_channels_) {
    if (kv.second->handle_messages_scheduled())
      return true;
  }
  return false;
}

uint64 GpuChannelManager::MessagesProcessed() {
  uint64 messages_processed = 0;
  for (auto& kv : gpu_channels_)
    messages_processed += kv.second->messages_processed();
  return messages_processed;
}

void GpuChannelManager::LoseAllContexts() {
  for (auto& kv : gpu_channels_)
    kv.second->MarkAllContextsLost();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&GpuChannelManager::OnLoseAllContexts,
                                    weak_factory_.GetWeakPtr()));
}

void GpuChannelManager::OnLoseAllContexts() {
  gpu_channels_.clear();
}

gfx::GLSurface* GpuChannelManager::GetDefaultOffscreenSurface() {
  if (!default_offscreen_surface_.get()) {
    default_offscreen_surface_ =
        gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size());
  }
  return default_offscreen_surface_.get();
}

}  // namespace content
