// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_watchdog.h"
#include "gpu/command_buffer/common/constants.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"

#if defined(OS_WIN)
#include "base/win/wrapped_window_proc.h"
#elif defined(TOUCH_UI)
#include "content/common/gpu/image_transport_surface_linux.h"
#endif

using gpu::Buffer;

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    GpuCommandBufferStub* share_group,
    gfx::PluginWindowHandle handle,
    const gfx::Size& size,
    const gpu::gles2::DisallowedExtensions& disallowed_extensions,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    int32 route_id,
    int32 renderer_id,
    int32 render_view_id,
    GpuWatchdog* watchdog,
    bool software)
    : channel_(channel),
      handle_(handle),
      initial_size_(size),
      disallowed_extensions_(disallowed_extensions),
      allowed_extensions_(allowed_extensions),
      requested_attribs_(attribs),
      route_id_(route_id),
      software_(software),
      last_flush_count_(0),
      renderer_id_(renderer_id),
      render_view_id_(render_view_id),
      parent_stub_for_initialization_(),
      parent_texture_for_initialization_(0),
      watchdog_(watchdog),
      task_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  if (share_group) {
    context_group_ = share_group->context_group_;
  } else {
    // TODO(gman): this needs to be false for everything but Pepper.
    bool bind_generates_resource = true;
    context_group_ = new gpu::gles2::ContextGroup(bind_generates_resource);
  }
}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  if (scheduler_.get()) {
    scheduler_->Destroy();
  }

  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DestroyCommandBuffer(
      handle_, renderer_id_, render_view_id_));
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Initialize,
                                    OnInitialize);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetParent,
                                    OnSetParent);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Flush, OnFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Rescheduled, OnRescheduled);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateTransferBuffer,
                                    OnCreateTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_RegisterTransferBuffer,
                                    OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_DestroyTransferBuffer,
                                    OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetTransferBuffer,
                                    OnGetTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoDecoder,
                                    OnCreateVideoDecoder)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyVideoDecoder,
                        OnDestroyVideoDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool GpuCommandBufferStub::IsScheduled() {
  return !scheduler_.get() || scheduler_->IsScheduled();
}

void GpuCommandBufferStub::OnInitializeFailed(IPC::Message* reply_message) {
  scheduler_.reset();
  command_buffer_.reset();
  GpuCommandBufferMsg_Initialize::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void GpuCommandBufferStub::OnInitialize(
    base::SharedMemoryHandle ring_buffer,
    int32 size,
    IPC::Message* reply_message) {
  DCHECK(!command_buffer_.get());

  command_buffer_.reset(new gpu::CommandBufferService);

#if defined(OS_WIN)
  // Windows dups the shared memory handle it receives into the current process
  // and closes it when this variable goes out of scope.
  base::SharedMemory shared_memory(ring_buffer,
                                   false,
                                   channel_->renderer_process());
#else
  // POSIX receives a dup of the shared memory handle and closes the dup when
  // this variable goes out of scope.
  base::SharedMemory shared_memory(ring_buffer, false);
#endif

  // Initialize the CommandBufferService and GpuScheduler.
  if (command_buffer_->Initialize(&shared_memory, size)) {
    scheduler_.reset(gpu::GpuScheduler::Create(command_buffer_.get(),
                                               channel_,
                                               context_group_.get()));
#if defined(TOUCH_UI)
    if (software_) {
      OnInitializeFailed(reply_message);
      return;
    }

    scoped_refptr<gfx::GLSurface> surface;
    if (handle_)
      surface = ImageTransportSurface::CreateSurface(this);
    else
      surface = gfx::GLSurface::CreateOffscreenGLSurface(software_,
                                                         gfx::Size(1, 1));
    if (!surface.get()) {
      LOG(ERROR) << "GpuCommandBufferStub: failed to create surface.";
      OnInitializeFailed(reply_message);
      return;
    }

    scoped_refptr<gfx::GLContext> context(
        gfx::GLContext::CreateGLContext(channel_->share_group(),
                                        surface.get()));
    if (!context.get()) {
      LOG(ERROR) << "GpuCommandBufferStub: failed to create context.";
      OnInitializeFailed(reply_message);
      return;
    }

    if (scheduler_->InitializeCommon(
        surface,
        context,
        initial_size_,
        disallowed_extensions_,
        allowed_extensions_.c_str(),
        requested_attribs_)) {
#else
    if (scheduler_->Initialize(
        handle_,
        initial_size_,
        software_,
        disallowed_extensions_,
        allowed_extensions_.c_str(),
        requested_attribs_,
        channel_->share_group())) {
#endif
      command_buffer_->SetPutOffsetChangeCallback(
          NewCallback(scheduler_.get(),
                      &gpu::GpuScheduler::PutChanged));
      command_buffer_->SetParseErrorCallback(
          NewCallback(this, &GpuCommandBufferStub::OnParseError));
      scheduler_->SetScheduledCallback(
          NewCallback(channel_, &GpuChannel::OnScheduled));

#if defined(OS_MACOSX)
      scheduler_->SetSwapBuffersCallback(
          NewCallback(this, &GpuCommandBufferStub::OnSwapBuffers));
#endif

      // On TOUCH_UI, the ImageTransportSurface handles co-ordinating the
      // resize with the browser process. The ImageTransportSurface sets it's
      // own resize callback, so we shouldn't do it here.
#if !defined(TOUCH_UI)
      if (handle_ != gfx::kNullPluginWindow) {
        scheduler_->SetResizeCallback(
            NewCallback(this, &GpuCommandBufferStub::OnResize));
      }
#endif
      if (watchdog_)
        scheduler_->SetCommandProcessedCallback(
            NewCallback(this, &GpuCommandBufferStub::OnCommandProcessed));

      if (parent_stub_for_initialization_) {
        scheduler_->SetParent(parent_stub_for_initialization_->scheduler_.get(),
                              parent_texture_for_initialization_);
        parent_stub_for_initialization_.reset();
        parent_texture_for_initialization_ = 0;
      }

    } else {
      OnInitializeFailed(reply_message);
      return;
    }
  }

  GpuCommandBufferMsg_Initialize::WriteReplyParams(reply_message, true);
  Send(reply_message);
}

void GpuCommandBufferStub::OnSetParent(int32 parent_route_id,
                                       uint32 parent_texture_id,
                                       IPC::Message* reply_message) {

  GpuCommandBufferStub* parent_stub = NULL;
  if (parent_route_id != MSG_ROUTING_NONE) {
    parent_stub = channel_->LookupCommandBuffer(parent_route_id);
  }

  bool result = true;
  if (scheduler_.get()) {
    gpu::GpuScheduler* parent_scheduler =
        parent_stub ? parent_stub->scheduler_.get() : NULL;
    result = scheduler_->SetParent(parent_scheduler, parent_texture_id);
  } else {
    // If we don't have a scheduler, it means that Initialize hasn't been called
    // yet. Keep around the requested parent stub and texture so that we can set
    // it in Initialize().
    parent_stub_for_initialization_ = parent_stub ?
        parent_stub->AsWeakPtr() : base::WeakPtr<GpuCommandBufferStub>();
    parent_texture_for_initialization_ = parent_texture_id;
  }
  GpuCommandBufferMsg_SetParent::WriteReplyParams(reply_message, result);
  Send(reply_message);
}

void GpuCommandBufferStub::OnGetState(IPC::Message* reply_message) {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kLostContext &&
      gfx::GLContext::LosesAllContextsOnContextLost())
    channel_->LoseAllContexts();

  GpuCommandBufferMsg_GetState::WriteReplyParams(reply_message, state);
  Send(reply_message);
}

void GpuCommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnParseError");
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::OnFlush(int32 put_offset,
                                   int32 last_known_get,
                                   uint32 flush_count,
                                   IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnFlush");
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (flush_count - last_flush_count_ >= 0x8000000U) {
    // We received this message out-of-order. This should not happen but is here
    // to catch regressions. Ignore the message.
    NOTREACHED() << "Received an AsyncFlush message out-of-order";
    GpuCommandBufferMsg_Flush::WriteReplyParams(reply_message, state);
    Send(reply_message);
  } else {
    last_flush_count_ = flush_count;

    // Reply immediately if the client was out of date with the current get
    // offset.
    bool reply_immediately = state.get_offset != last_known_get;
    if (reply_immediately) {
      GpuCommandBufferMsg_Flush::WriteReplyParams(reply_message, state);
      Send(reply_message);
    }

    // Process everything up to the put offset.
    state = command_buffer_->FlushSync(put_offset, last_known_get);

    // Lose all contexts if the context was lost.
    if (state.error == gpu::error::kLostContext &&
        gfx::GLContext::LosesAllContextsOnContextLost()) {
      channel_->LoseAllContexts();
    }

    // Then if the client was up-to-date with the get offset, reply to the
    // synchronpous IPC only after processing all commands are processed. This
    // prevents the client from "spinning" when it fills up the command buffer.
    // Otherwise, since the state has changed since the immediate reply, send
    // an asyncronous state update back to the client.
    if (!reply_immediately) {
      GpuCommandBufferMsg_Flush::WriteReplyParams(reply_message, state);
      Send(reply_message);
    } else {
      ReportState();
    }
  }
}

void GpuCommandBufferStub::OnAsyncFlush(int32 put_offset, uint32 flush_count) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnAsyncFlush");
  if (flush_count - last_flush_count_ < 0x8000000U) {
    last_flush_count_ = flush_count;
    command_buffer_->Flush(put_offset);
  } else {
    // We received this message out-of-order. This should not happen but is here
    // to catch regressions. Ignore the message.
    NOTREACHED() << "Received a Flush message out-of-order";
  }

  ReportState();
}

void GpuCommandBufferStub::OnRescheduled() {
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  command_buffer_->Flush(state.put_offset);

  ReportState();
}

void GpuCommandBufferStub::OnCreateTransferBuffer(int32 size,
                                                  int32 id_request,
                                                  IPC::Message* reply_message) {
  int32 id = command_buffer_->CreateTransferBuffer(size, id_request);
  GpuCommandBufferMsg_CreateTransferBuffer::WriteReplyParams(reply_message, id);
  Send(reply_message);
}

void GpuCommandBufferStub::OnRegisterTransferBuffer(
    base::SharedMemoryHandle transfer_buffer,
    size_t size,
    int32 id_request,
    IPC::Message* reply_message) {
#if defined(OS_WIN)
  // Windows dups the shared memory handle it receives into the current process
  // and closes it when this variable goes out of scope.
  base::SharedMemory shared_memory(transfer_buffer,
                                   false,
                                   channel_->renderer_process());
#else
  // POSIX receives a dup of the shared memory handle and closes the dup when
  // this variable goes out of scope.
  base::SharedMemory shared_memory(transfer_buffer, false);
#endif

  int32 id = command_buffer_->RegisterTransferBuffer(&shared_memory,
                                                     size,
                                                     id_request);

  GpuCommandBufferMsg_RegisterTransferBuffer::WriteReplyParams(reply_message,
                                                               id);
  Send(reply_message);
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(
    int32 id,
    IPC::Message* reply_message) {
  command_buffer_->DestroyTransferBuffer(id);
  Send(reply_message);
}

void GpuCommandBufferStub::OnGetTransferBuffer(
    int32 id,
    IPC::Message* reply_message) {
  base::SharedMemoryHandle transfer_buffer = base::SharedMemoryHandle();
  uint32 size = 0;

  // Fail if the renderer process has not provided its process handle.
  if (!channel_->renderer_process())
    return;

  Buffer buffer = command_buffer_->GetTransferBuffer(id);
  if (buffer.shared_memory) {
    // Assume service is responsible for duplicating the handle to the calling
    // process.
    buffer.shared_memory->ShareToProcess(channel_->renderer_process(),
                                         &transfer_buffer);
    size = buffer.size;
  }

  GpuCommandBufferMsg_GetTransferBuffer::WriteReplyParams(reply_message,
                                                          transfer_buffer,
                                                          size);
  Send(reply_message);
}

#if defined(OS_MACOSX)
void GpuCommandBufferStub::OnSwapBuffers() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSwapBuffers");
  if (handle_) {
    // To swap on OSX, we have to send a message to the browser to get the
    // context put onscreen.
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
    params.renderer_id = renderer_id_;
    params.render_view_id = render_view_id_;
    params.window = handle_;
    params.surface_id = scheduler_->GetSurfaceId();
    params.route_id = route_id();
    params.swap_buffers_count = scheduler_->swap_buffers_count();
    gpu_channel_manager->Send(
        new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
    scheduler_->SetScheduled(false);
  }
}
#endif

void GpuCommandBufferStub::OnCommandProcessed() {
  if (watchdog_)
    watchdog_->CheckArmed();
}

#if defined(OS_MACOSX)
void GpuCommandBufferStub::AcceleratedSurfaceBuffersSwapped(
    uint64 swap_buffers_count) {
  TRACE_EVENT1("gpu",
               "GpuCommandBufferStub::AcceleratedSurfaceBuffersSwapped",
               "frame", swap_buffers_count);
  DCHECK(handle_ != gfx::kNullPluginWindow);

  // Multiple swapbuffers may get consolidated together into a single
  // AcceleratedSurfaceBuffersSwapped call. Upstream code listening to the
  // GpuCommandBufferMsg_SwapBuffers expects to be called one time for every
  // swap. So, send one SwapBuffers message for every outstanding swap.
  uint64 delta = swap_buffers_count -
      scheduler_->acknowledged_swap_buffers_count();
  scheduler_->set_acknowledged_swap_buffers_count(swap_buffers_count);

  for(uint64 i = 0; i < delta; i++) {
    // Wake up the GpuScheduler to start doing work again. When the scheduler
    // wakes up, it will send any deferred echo acknowledgements, triggering
    // associated swapbuffer callbacks.
    scheduler_->SetScheduled(true);
  }
}
#endif  // defined(OS_MACOSX)

void GpuCommandBufferStub::OnResize(gfx::Size size) {
  if (handle_ == gfx::kNullPluginWindow)
    return;

#if defined(OS_MACOSX)
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();

  // On Mac, we need to tell the browser about the new IOSurface handle,
  // asynchronously.
  uint64 new_backing_store = scheduler_->SetWindowSizeForIOSurface(size);
  if (new_backing_store) {
    GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params params;
    params.renderer_id = renderer_id_;
    params.render_view_id = render_view_id_;
    params.window = handle_;
    params.width = size.width();
    params.height = size.height();
    params.identifier = new_backing_store;
    gpu_channel_manager->Send(
        new GpuHostMsg_AcceleratedSurfaceSetIOSurface(params));
  } else {
    // TODO(kbr): figure out what to do here. It wouldn't be difficult
    // to support the compositor on 10.5, but the performance would be
    // questionable.
    NOTREACHED();
  }
#elif defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI) || defined(OS_WIN)
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();

  // On Windows, Linux, we need to coordinate resizing of onscreen
  // contexts with the resizing of the actual OS-level window. We do this by
  // sending a resize message to the browser process and descheduling the
  // context until the ViewResized message comes back in reply.
  // Send the resize message if needed
  gpu_channel_manager->Send(
      new GpuHostMsg_ResizeView(renderer_id_,
                                render_view_id_,
                                route_id_,
                                size));

  scheduler_->SetScheduled(false);
#endif // defined(OS_MACOSX)
}


void GpuCommandBufferStub::ViewResized() {
#if defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI) || defined(OS_WIN)
  DCHECK(handle_ != gfx::kNullPluginWindow);
  scheduler_->SetScheduled(true);

  // Recreate the view surface to match the window size. TODO(apatrick): this is
  // likely not necessary on all platforms.
  gfx::GLContext* context = scheduler_->decoder()->GetGLContext();
  gfx::GLSurface* surface = scheduler_->decoder()->GetGLSurface();
  context->ReleaseCurrent(surface);
  if (surface) {
    surface->Destroy();
    surface->Initialize();
  }
#endif
}

void GpuCommandBufferStub::ReportState() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kLostContext &&
      gfx::GLContext::LosesAllContextsOnContextLost()) {
    channel_->LoseAllContexts();
  } else {
    IPC::Message* msg = new GpuCommandBufferMsg_UpdateState(route_id_, state);
    msg->set_unblock(true);
    Send(msg);
  }
}

void GpuCommandBufferStub::OnCreateVideoDecoder(
    media::VideoDecodeAccelerator::Profile profile,
    IPC::Message* reply_message) {
  int decoder_route_id = channel_->GenerateRouteID();
  GpuCommandBufferMsg_CreateVideoDecoder::WriteReplyParams(
      reply_message, decoder_route_id);
  GpuVideoDecodeAccelerator* decoder =
      new GpuVideoDecodeAccelerator(this, decoder_route_id, this);
  video_decoders_.AddWithID(decoder, decoder_route_id);
  channel_->AddRoute(decoder_route_id, decoder);
  decoder->Initialize(profile, reply_message);
}

void GpuCommandBufferStub::OnDestroyVideoDecoder(int decoder_route_id) {
  channel_->RemoveRoute(decoder_route_id);
  video_decoders_.Remove(decoder_route_id);
}

#endif  // defined(ENABLE_GPU)
