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
#endif

using gpu::Buffer;

#if defined(OS_WIN)
#define kCompositorWindowOwner L"CompositorWindowOwner"
#endif  // defined(OS_WIN)

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    gfx::PluginWindowHandle handle,
    GpuCommandBufferStub* parent,
    const gfx::Size& size,
    const gpu::gles2::DisallowedExtensions& disallowed_extensions,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    uint32 parent_texture_id,
    int32 route_id,
    int32 renderer_id,
    int32 render_view_id,
    GpuWatchdog* watchdog)
    : channel_(channel),
      handle_(handle),
      parent_(
          parent ? parent->AsWeakPtr() : base::WeakPtr<GpuCommandBufferStub>()),
      initial_size_(size),
      disallowed_extensions_(disallowed_extensions),
      allowed_extensions_(allowed_extensions),
      requested_attribs_(attribs),
      parent_texture_id_(parent_texture_id),
      route_id_(route_id),
      renderer_id_(renderer_id),
      render_view_id_(render_view_id),
      watchdog_(watchdog),
      task_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
  // If the scheduler is unscheduled, defer sync and async messages until it is
  // rescheduled. Also, even if the scheduler is scheduled, do not allow newly
  // received messages to be handled before previously received deferred ones;
  // append them to the deferred queue as well.
  if ((scheduler_.get() && !scheduler_->IsScheduled()) ||
      !deferred_messages_.empty()) {
    deferred_messages_.push(new IPC::Message(message));
    return true;
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Initialize,
                                    OnInitialize);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Flush, OnFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateTransferBuffer,
                                    OnCreateTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_RegisterTransferBuffer,
                                    OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_DestroyTransferBuffer,
                                    OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetTransferBuffer,
                                    OnGetTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ResizeOffscreenFrameBuffer,
                        OnResizeOffscreenFrameBuffer);
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetWindowSize, OnSetWindowSize);
#endif  // defined(OS_MACOSX)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

void GpuCommandBufferStub::OnInitialize(
    base::SharedMemoryHandle ring_buffer,
    int32 size,
    IPC::Message* reply_message) {
  DCHECK(!command_buffer_.get());

  bool result = false;

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
    gpu::GpuScheduler* parent_processor =
        parent_ ? parent_->scheduler_.get() : NULL;
    scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(), NULL));
    if (scheduler_->Initialize(
        handle_,
        initial_size_,
        disallowed_extensions_,
        allowed_extensions_.c_str(),
        requested_attribs_,
        parent_processor,
        parent_texture_id_)) {
      command_buffer_->SetPutOffsetChangeCallback(
          NewCallback(scheduler_.get(),
                      &gpu::GpuScheduler::PutChanged));
      scheduler_->SetSwapBuffersCallback(
          NewCallback(this, &GpuCommandBufferStub::OnSwapBuffers));
      scheduler_->SetLatchCallback(base::Bind(
          &GpuChannel::OnLatchCallback, base::Unretained(channel_), route_id_));
      scheduler_->SetScheduledCallback(
          NewCallback(this, &GpuCommandBufferStub::OnScheduled));
      if (watchdog_)
        scheduler_->SetCommandProcessedCallback(
            NewCallback(this, &GpuCommandBufferStub::OnCommandProcessed));

#if defined(OS_MACOSX)
      if (handle_) {
        // This context conceptually puts its output directly on the
        // screen, rendered by the accelerated plugin layer in
        // RenderWidgetHostViewMac. Set up a pathway to notify the
        // browser process when its contents change.
        scheduler_->SetSwapBuffersCallback(
            NewCallback(this,
                        &GpuCommandBufferStub::SwapBuffersCallback));
      }
#endif  // defined(OS_MACOSX)

      // Set up a pathway for resizing the output window or framebuffer at the
      // right time relative to other GL commands.
      scheduler_->SetResizeCallback(
          NewCallback(this, &GpuCommandBufferStub::ResizeCallback));

      result = true;
    } else {
      scheduler_.reset();
      command_buffer_.reset();
    }
  }

  GpuCommandBufferMsg_Initialize::WriteReplyParams(reply_message, result);
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

void GpuCommandBufferStub::OnFlush(int32 put_offset,
                                   int32 last_known_get,
                                   IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnFlush");
  gpu::CommandBuffer::State state = command_buffer_->FlushSync(put_offset,
                                                               last_known_get);
  if (state.error == gpu::error::kLostContext &&
      gfx::GLContext::LosesAllContextsOnContextLost())
    channel_->LoseAllContexts();

  GpuCommandBufferMsg_Flush::WriteReplyParams(reply_message, state);
  Send(reply_message);
}

void GpuCommandBufferStub::OnAsyncFlush(int32 put_offset) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnAsyncFlush");
  command_buffer_->Flush(put_offset);
  // TODO(piman): Do this everytime the scheduler finishes processing a batch of
  // commands.
  MessageLoop::current()->PostTask(FROM_HERE,
      task_factory_.NewRunnableMethod(&GpuCommandBufferStub::ReportState));
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

void GpuCommandBufferStub::OnResizeOffscreenFrameBuffer(const gfx::Size& size) {
  scheduler_->ResizeOffscreenFrameBuffer(size);
}

void GpuCommandBufferStub::OnSwapBuffers() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSwapBuffers");
  ReportState();
  Send(new GpuCommandBufferMsg_SwapBuffers(route_id_));
}

void GpuCommandBufferStub::OnCommandProcessed() {
  if (watchdog_)
    watchdog_->CheckArmed();
}

void GpuCommandBufferStub::HandleDeferredMessages() {
  // Empty the deferred queue so OnMessageRecieved does not defer on that
  // account and to prevent an infinite loop if the scheduler is unscheduled
  // as a result of handling already deferred messages.
  std::queue<IPC::Message*> deferred_messages_copy;
  std::swap(deferred_messages_copy, deferred_messages_);

  while (!deferred_messages_copy.empty()) {
    scoped_ptr<IPC::Message> message(deferred_messages_copy.front());
    deferred_messages_copy.pop();

    OnMessageReceived(*message);
  }
}

void GpuCommandBufferStub::OnScheduled() {
  // Post a task to handle any deferred messages. The deferred message queue is
  // not emptied here, which ensures that OnMessageReceived will continue to
  // defer newly received messages until the ones in the queue have all been
  // handled by HandleDeferredMessages. HandleDeferredMessages is invoked as a
  // task to prevent reentrancy.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(
          &GpuCommandBufferStub::HandleDeferredMessages));
}

#if defined(OS_MACOSX)
void GpuCommandBufferStub::OnSetWindowSize(const gfx::Size& size) {
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  // Try using the IOSurface version first.
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
}

void GpuCommandBufferStub::SwapBuffersCallback() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::SwapBuffersCallback");
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

void GpuCommandBufferStub::AcceleratedSurfaceBuffersSwapped(
    uint64 swap_buffers_count) {
  TRACE_EVENT0("gpu",
               "GpuCommandBufferStub::AcceleratedSurfaceBuffersSwapped");

  // Multiple swapbuffers may get consolidated together into a single
  // AcceleratedSurfaceBuffersSwapped call. Since OnSwapBuffers expects to be
  // called one time for every swap, make up the difference here.
  uint64 delta = swap_buffers_count -
      scheduler_->acknowledged_swap_buffers_count();

  scheduler_->set_acknowledged_swap_buffers_count(swap_buffers_count);

  for(uint64 i = 0; i < delta; i++)
    OnSwapBuffers();

  // Wake up the GpuScheduler to start doing work again.
  scheduler_->SetScheduled(true);
}
#endif  // defined(OS_MACOSX)

void GpuCommandBufferStub::ResizeCallback(gfx::Size size) {
  if (handle_ == gfx::kNullPluginWindow) {
    scheduler_->decoder()->ResizeOffscreenFrameBuffer(size);
    scheduler_->decoder()->UpdateOffscreenFrameBufferSize();
  } else {
#if defined(OS_LINUX) && !defined(TOUCH_UI) || defined(OS_WIN)
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    gpu_channel_manager->Send(
        new GpuHostMsg_ResizeView(renderer_id_,
                                  render_view_id_,
                                  route_id_,
                                  size));

    scheduler_->SetScheduled(false);
#endif
  }
}

void GpuCommandBufferStub::ViewResized() {
#if defined(OS_LINUX) && !defined(TOUCH_UI) || defined(OS_WIN)
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

#endif  // defined(ENABLE_GPU)
