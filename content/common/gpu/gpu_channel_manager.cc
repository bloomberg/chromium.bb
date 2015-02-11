// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/message_router.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "ipc/message_filter.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_share_group.h"
#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

namespace {

class GpuChannelManagerMessageFilter : public IPC::MessageFilter {
 public:
  GpuChannelManagerMessageFilter(
      GpuMemoryBufferFactory* gpu_memory_buffer_factory)
      : sender_(NULL), gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {}

  void OnFilterAdded(IPC::Sender* sender) override {
    DCHECK(!sender_);
    sender_ = sender;
  }

  void OnFilterRemoved() override {
    DCHECK(sender_);
    sender_ = NULL;
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    DCHECK(sender_);
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(GpuChannelManagerMessageFilter, message)
      IPC_MESSAGE_HANDLER(GpuMsg_CreateGpuMemoryBuffer, OnCreateGpuMemoryBuffer)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 protected:
  ~GpuChannelManagerMessageFilter() override {}

  void OnCreateGpuMemoryBuffer(
      const GpuMsg_CreateGpuMemoryBuffer_Params& params) {
    TRACE_EVENT2("gpu",
                 "GpuChannelManagerMessageFilter::OnCreateGpuMemoryBuffer",
                 "id", params.id, "client_id", params.client_id);
    sender_->Send(new GpuHostMsg_GpuMemoryBufferCreated(
        gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
            params.id, params.size, params.format, params.usage,
            params.client_id, params.surface_handle)));
  }

  IPC::Sender* sender_;
  GpuMemoryBufferFactory* gpu_memory_buffer_factory_;
};

gfx::GpuMemoryBufferType GetGpuMemoryBufferFactoryType() {
  std::vector<gfx::GpuMemoryBufferType> supported_types;
  GpuMemoryBufferFactory::GetSupportedTypes(&supported_types);
  DCHECK(!supported_types.empty());
  return supported_types[0];
}

}  // namespace

GpuChannelManager::GpuChannelManager(MessageRouter* router,
                                     GpuWatchdog* watchdog,
                                     base::MessageLoopProxy* io_message_loop,
                                     base::WaitableEvent* shutdown_event,
                                     IPC::SyncChannel* channel)
    : io_message_loop_(io_message_loop),
      shutdown_event_(shutdown_event),
      router_(router),
      gpu_memory_manager_(
          this,
          GpuMemoryManager::kDefaultMaxSurfacesWithFrontbufferSoftLimit),
      watchdog_(watchdog),
      sync_point_manager_(gpu::SyncPointManager::Create(false)),
      gpu_memory_buffer_factory_(
          GpuMemoryBufferFactory::Create(GetGpuMemoryBufferFactoryType())),
      channel_(channel),
      filter_(
          new GpuChannelManagerMessageFilter(gpu_memory_buffer_factory_.get())),
      relinquish_resources_pending_(false),
      weak_factory_(this) {
  DCHECK(router_);
  DCHECK(io_message_loop);
  DCHECK(shutdown_event);
  channel_->AddFilter(filter_.get());
}

GpuChannelManager::~GpuChannelManager() {
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
  CheckRelinquishGpuResources();
}

int GpuChannelManager::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannelManager::AddRoute(int32 routing_id, IPC::Listener* listener) {
  router_->AddRoute(routing_id, listener);
}

void GpuChannelManager::RemoveRoute(int32 routing_id) {
  router_->RemoveRoute(routing_id);
}

GpuChannel* GpuChannelManager::LookupChannel(int32 client_id) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter == gpu_channels_.end())
    return NULL;
  else
    return iter->second;
}

bool GpuChannelManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannelManager, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel, OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateViewCommandBuffer,
                        OnCreateViewCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_DestroyGpuMemoryBuffer, OnDestroyGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_LoadedShader, OnLoadedShader)
    IPC_MESSAGE_HANDLER(GpuMsg_RelinquishResources, OnRelinquishResources)
    IPC_MESSAGE_HANDLER(GpuMsg_UpdateValueState, OnUpdateValueState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool GpuChannelManager::Send(IPC::Message* msg) { return router_->Send(msg); }

void GpuChannelManager::OnEstablishChannel(int client_id,
                                           bool share_context,
                                           bool allow_future_sync_points) {
  IPC::ChannelHandle channel_handle;

  gfx::GLShareGroup* share_group = NULL;
  gpu::gles2::MailboxManager* mailbox_manager = NULL;
  if (share_context) {
    if (!share_group_.get()) {
      share_group_ = new gfx::GLShareGroup;
      DCHECK(!mailbox_manager_.get());
      mailbox_manager_ = new gpu::gles2::MailboxManagerImpl;
    }
    share_group = share_group_.get();
    mailbox_manager = mailbox_manager_.get();
  }

  scoped_ptr<GpuChannel> channel(new GpuChannel(this,
                                                watchdog_,
                                                share_group,
                                                mailbox_manager,
                                                client_id,
                                                false,
                                                allow_future_sync_points));
  channel->Init(io_message_loop_.get(), shutdown_event_);
  channel_handle.name = channel->GetChannelName();

#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
  // that it gets closed after it has been sent.
  base::ScopedFD renderer_fd = channel->TakeRendererFileDescriptor();
  DCHECK(renderer_fd.is_valid());
  channel_handle.socket = base::FileDescriptor(renderer_fd.Pass());
#endif

  gpu_channels_.set(client_id, channel.Pass());

  Send(new GpuHostMsg_ChannelEstablished(channel_handle));
}

void GpuChannelManager::OnCloseChannel(
    const IPC::ChannelHandle& channel_handle) {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    if (iter->second->GetChannelName() == channel_handle.name) {
      gpu_channels_.erase(iter);
      CheckRelinquishGpuResources();
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

  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter != gpu_channels_.end()) {
    result = iter->second->CreateViewCommandBuffer(
        window, surface_id, init_params, route_id);
  }

  Send(new GpuHostMsg_CommandBufferCreated(result));
}

void GpuChannelManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannelManager::DestroyGpuMemoryBufferOnIO,
                 base::Unretained(this),
                 id,
                 client_id));
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
  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter != gpu_channels_.end()) {
    iter->second->HandleUpdateValueState(target, state);
  }
}

void GpuChannelManager::OnLoadedShader(std::string program_proto) {
  if (program_cache())
    program_cache()->LoadProgram(program_proto);
}

bool GpuChannelManager::HandleMessagesScheduled() {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    if (iter->second->handle_messages_scheduled())
      return true;
  }
  return false;
}

uint64 GpuChannelManager::MessagesProcessed() {
  uint64 messages_processed = 0;

  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    messages_processed += iter->second->messages_processed();
  }
  return messages_processed;
}

void GpuChannelManager::LoseAllContexts() {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    iter->second->MarkAllContextsLost();
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannelManager::OnLoseAllContexts,
                 weak_factory_.GetWeakPtr()));
}

void GpuChannelManager::OnLoseAllContexts() {
  gpu_channels_.clear();
  CheckRelinquishGpuResources();
}

gfx::GLSurface* GpuChannelManager::GetDefaultOffscreenSurface() {
  if (!default_offscreen_surface_.get()) {
    default_offscreen_surface_ =
        gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size());
  }
  return default_offscreen_surface_.get();
}

void GpuChannelManager::OnRelinquishResources() {
  relinquish_resources_pending_ = true;
  CheckRelinquishGpuResources();
}

void GpuChannelManager::CheckRelinquishGpuResources() {
  if (relinquish_resources_pending_ && gpu_channels_.size() <= 1) {
    relinquish_resources_pending_ = false;
    if (default_offscreen_surface_.get()) {
      default_offscreen_surface_->DestroyAndTerminateDisplay();
      default_offscreen_surface_ = NULL;
    }
#if defined(USE_OZONE)
    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupport()
        ->RelinquishGpuResources(
            base::Bind(&GpuChannelManager::OnResourcesRelinquished,
                       weak_factory_.GetWeakPtr()));
#else
    OnResourcesRelinquished();
#endif
  }
}

void GpuChannelManager::OnResourcesRelinquished() {
  Send(new GpuHostMsg_ResourcesRelinquished());
}

}  // namespace content
