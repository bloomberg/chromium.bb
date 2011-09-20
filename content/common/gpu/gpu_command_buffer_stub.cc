// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_watchdog.h"
#include "gpu/command_buffer/common/constants.h"
#include "ui/gfx/gl/gl_switches.h"

#if defined(TOUCH_UI)
#include "content/common/gpu/image_transport_surface_linux.h"
#endif

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
#if defined(OS_MACOSX)
      swap_buffers_count_(0),
      acknowledged_swap_buffers_count_(0),
#endif
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
  Destroy();

  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DestroyCommandBuffer(
      handle_, renderer_id_, render_view_id_));
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current.
  if (decoder_.get()) {
    if (!decoder_->MakeCurrent()) {
      LOG(ERROR) << "Context lost because MakeCurrent failed.";
      command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(gpu::error::kLostContext);
      return false;
    }
  }

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

#if defined(OS_MACOSX)

TransportDIB::Handle GpuCommandBufferStub::SetWindowSizeForTransportDIB(
    const gfx::Size& size) {
  return accelerated_surface_->SetTransportDIBSize(size);
}

void GpuCommandBufferStub::SetTransportDIBAllocAndFree(
    Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
    Callback1<TransportDIB::Id>::Type* deallocator) {
  accelerated_surface_->SetTransportDIBAllocAndFree(allocator, deallocator);
}

#endif  // OS_MACOSX

void GpuCommandBufferStub::Destroy() {
  // The scheduler has raw references to the decoder and the command buffer so
  // destroy it before those.
  scheduler_.reset();

  if (decoder_.get()) {
    decoder_->Destroy();
    decoder_.reset();
  }

  command_buffer_.reset();

  context_ = NULL;
  surface_ = NULL;

#if defined(OS_MACOSX)
  accelerated_surface_.reset();
#endif
}

void GpuCommandBufferStub::OnInitializeFailed(IPC::Message* reply_message) {
  Destroy();
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

  if (!command_buffer_->Initialize(&shared_memory, size)) {
    OnInitializeFailed(reply_message);
    return;
  }

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group_.get()));

  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(),
                                         decoder_.get(),
                                         NULL));

  decoder_->set_engine(scheduler_.get());

  if (handle_) {
#if defined(TOUCH_UI)
    if (software_) {
      OnInitializeFailed(reply_message);
      return;
    }

    surface_ = ImageTransportSurface::CreateSurface(
        channel_->gpu_channel_manager(),
        render_view_id_,
        renderer_id_,
        route_id_);
#elif defined(OS_WIN) || defined(OS_LINUX)
    surface_ = gfx::GLSurface::CreateViewGLSurface(software_, handle_);
#elif defined(OS_MACOSX)
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(software_,
                                                        gfx::Size(1, 1));
#endif
  } else {
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(software_,
                                                        gfx::Size(1, 1));
  }

  if (!surface_.get()) {
    LOG(ERROR) << "Failed to create surface.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  context_ = gfx::GLContext::CreateGLContext(channel_->share_group(),
                                             surface_.get());
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create context.\n";
    OnInitializeFailed(reply_message);
    return;
  }

#if defined(OS_MACOSX)
  // On Mac OS X since we can not render on-screen we don't even
  // attempt to create a view based GLContext. The only difference
  // between "on-screen" and "off-screen" rendering on this platform
  // is whether we allocate an AcceleratedSurface, which transmits the
  // rendering results back to the browser.
  if (handle_) {
    accelerated_surface_.reset(new AcceleratedSurface());

    // Note that although the GLContext is passed to Initialize and the
    // GLContext will later be owned by the decoder, AcceleratedSurface does
    // not hold on to the reference. It simply extracts the underlying GL
    // context in order to share the namespace with another context.
    if (!accelerated_surface_->Initialize(context_.get(), false)) {
      LOG(ERROR) << "Failed to initialize AcceleratedSurface.";
      OnInitializeFailed(reply_message);
      return;
    }
  }
#endif

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(surface_.get(),
                            context_.get(),
                            initial_size_,
                            disallowed_extensions_,
                            allowed_extensions_.c_str(),
                            requested_attribs_)) {
    LOG(ERROR) << "Failed to initialize decoder.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceLogging)) {
    decoder_->set_debug(true);
  }

  command_buffer_->SetPutOffsetChangeCallback(
      NewCallback(scheduler_.get(),
                  &gpu::GpuScheduler::PutChanged));
  command_buffer_->SetParseErrorCallback(
      NewCallback(this, &GpuCommandBufferStub::OnParseError));
  scheduler_->SetScheduledCallback(
      NewCallback(channel_, &GpuChannel::OnScheduled));

#if defined(OS_MACOSX)
  decoder_->SetSwapBuffersCallback(
      NewCallback(this, &GpuCommandBufferStub::OnSwapBuffers));
#endif

  // On TOUCH_UI, the ImageTransportSurface handles co-ordinating the
  // resize with the browser process. The ImageTransportSurface sets it's
  // own resize callback, so we shouldn't do it here.
#if !defined(TOUCH_UI)
  if (handle_ != gfx::kNullPluginWindow) {
    decoder_->SetResizeCallback(
        NewCallback(this, &GpuCommandBufferStub::OnResize));
  }
#endif

  if (watchdog_) {
    scheduler_->SetCommandProcessedCallback(
        NewCallback(this, &GpuCommandBufferStub::OnCommandProcessed));
  }

  if (parent_stub_for_initialization_) {
    decoder_->SetParent(parent_stub_for_initialization_->decoder_.get(),
                        parent_texture_for_initialization_);
    parent_stub_for_initialization_.reset();
    parent_texture_for_initialization_ = 0;
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
    gpu::gles2::GLES2Decoder* parent_decoder =
        parent_stub ? parent_stub->decoder_.get() : NULL;
    result = decoder_->SetParent(parent_decoder, parent_texture_id);
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

  gpu::Buffer buffer = command_buffer_->GetTransferBuffer(id);
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

  DCHECK(decoder_.get());
  DCHECK(decoder_->GetGLContext());
  DCHECK(decoder_->GetGLContext()->IsCurrent(decoder_->GetGLSurface()));

  ++swap_buffers_count_;

  if (accelerated_surface_.get()) {
    accelerated_surface_->SwapBuffers();
  }

  if (handle_) {
    // To swap on OSX, we have to send a message to the browser to get the
    // context put onscreen.
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
    params.renderer_id = renderer_id_;
    params.render_view_id = render_view_id_;
    params.window = handle_;
    params.surface_id = GetSurfaceId();
    params.route_id = route_id();
    params.swap_buffers_count = swap_buffers_count_;
    gpu_channel_manager->Send(
        new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
    scheduler_->SetScheduled(false);
  }
}

uint64 GpuCommandBufferStub::GetSurfaceId() {
  if (!accelerated_surface_.get())
    return 0;
  return accelerated_surface_->GetSurfaceId();
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
  uint64 delta = swap_buffers_count - acknowledged_swap_buffers_count_;
  acknowledged_swap_buffers_count_ = swap_buffers_count;

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
  uint64 new_backing_store = accelerated_surface_->SetSurfaceSize(size);
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
#endif

#if defined(OS_WIN)
  // Recreate the view surface to match the window size. Swap chains do not
  // automatically resize with window size with D3D.
  context_->ReleaseCurrent(surface_.get());
  if (surface_.get()) {
    surface_->Destroy();
    surface_->Initialize();
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
