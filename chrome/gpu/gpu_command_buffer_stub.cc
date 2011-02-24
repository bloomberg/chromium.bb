// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "base/process_util.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_command_buffer_stub.h"
#include "chrome/gpu/gpu_thread.h"

using gpu::Buffer;

#if defined(OS_WIN)
#define kCompositorWindowOwner L"CompositorWindowOwner"
#endif  // defined(OS_WIN)

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    gfx::PluginWindowHandle handle,
    GpuCommandBufferStub* parent,
    const gfx::Size& size,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    uint32 parent_texture_id,
    int32 route_id,
    int32 renderer_id,
    int32 render_view_id)
    : channel_(channel),
      handle_(handle),
      parent_(
          parent ? parent->AsWeakPtr() : base::WeakPtr<GpuCommandBufferStub>()),
      initial_size_(size),
      allowed_extensions_(allowed_extensions),
      requested_attribs_(attribs),
      parent_texture_id_(parent_texture_id),
      route_id_(route_id),
#if defined(OS_WIN)
      compositor_window_(NULL),
#endif  // defined(OS_WIN)
      renderer_id_(renderer_id),
      render_view_id_(render_view_id) {
}

#if defined(OS_WIN)
static LRESULT CALLBACK CompositorWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
  switch (message) {
    case WM_ERASEBKGND:
      return 0;
    case WM_DESTROY:
      RemoveProp(hwnd, kCompositorWindowOwner);
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(hwnd, &paint);
      if (dc) {
        HANDLE h = GetProp(hwnd, kCompositorWindowOwner);
        if (h) {
          GpuCommandBufferStub* stub =
              reinterpret_cast<GpuCommandBufferStub*>(h);
          stub->OnCompositorWindowPainted();
        }
        EndPaint(hwnd, &paint);
      }
      break;
    }
    default:
      return DefWindowProc(hwnd, message, wparam, lparam);
  }
  return 0;
}

bool GpuCommandBufferStub::CreateCompositorWindow() {
  DCHECK(handle_ != gfx::kNullPluginWindow);
  HWND host_window = static_cast<HWND>(handle_);

  // Create the compositor window itself.
  DCHECK(host_window);
  static ATOM window_class = 0;
  if (!window_class) {
    WNDCLASSEX wcex;
    wcex.cbSize         = sizeof(wcex);
    wcex.style          = 0;
    wcex.lpfnWndProc    = CompositorWindowProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = GetModuleHandle(NULL);
    wcex.hIcon          = 0;
    wcex.hCursor        = 0;
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = L"CompositorWindowClass";
    wcex.hIconSm        = 0;
    window_class = RegisterClassEx(&wcex);
    DCHECK(window_class);
  }

  HWND compositor_window = CreateWindowEx(
      WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
      MAKEINTATOM(window_class),
      0,
      WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_DISABLED,
      0, 0,
      0, 0,
      host_window,
      0,
      GetModuleHandle(NULL),
      0);
  if (!compositor_window) {
    compositor_window_ = gfx::kNullPluginWindow;
    return false;
  }
  SetProp(compositor_window, kCompositorWindowOwner,
      reinterpret_cast<HANDLE>(this));

  RECT parent_rect;
  GetClientRect(host_window, &parent_rect);

  UINT flags = SWP_NOSENDCHANGING | SWP_NOCOPYBITS | SWP_NOZORDER |
      SWP_NOACTIVATE | SWP_DEFERERASE | SWP_SHOWWINDOW;
  SetWindowPos(compositor_window,
      NULL,
      0, 0,
      parent_rect.right - parent_rect.left,
      parent_rect.bottom - parent_rect.top,
      flags);
  compositor_window_ = static_cast<gfx::PluginWindowHandle>(compositor_window);
  return true;
}

void GpuCommandBufferStub::OnCompositorWindowPainted() {
  GpuThread* gpu_thread = channel_->gpu_thread();
  gpu_thread->Send(new GpuHostMsg_ScheduleComposite(
      renderer_id_, render_view_id_));
}
#endif  // defined(OS_WIN)


GpuCommandBufferStub::~GpuCommandBufferStub() {
  if (processor_.get()) {
    processor_->Destroy();
  }
#if defined(OS_WIN)
  if (compositor_window_) {
    DestroyWindow(static_cast<HWND>(compositor_window_));
    compositor_window_ = NULL;
  }
#endif  // defined(OS_WIN)

  GpuThread* gpu_thread = channel_->gpu_thread();
  gpu_thread->Send(new GpuHostMsg_DestroyCommandBuffer(
      handle_, renderer_id_, render_view_id_));
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Initialize, OnInitialize);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncGetState, OnAsyncGetState);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Flush, OnFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateTransferBuffer,
                        OnCreateTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterTransferBuffer,
                        OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GetTransferBuffer,
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
    int32 size,
    base::SharedMemoryHandle* ring_buffer) {
  DCHECK(!command_buffer_.get());

  *ring_buffer = base::SharedMemory::NULLHandle();

  command_buffer_.reset(new gpu::CommandBufferService);

  // Create the child window, if needed
#if defined(OS_WIN)
  gfx::PluginWindowHandle output_window_handle;
  if (handle_) {
    if (!CreateCompositorWindow()) {
      return;
    }
    output_window_handle = compositor_window_;
  } else {
    output_window_handle = handle_;
  }
#else
  gfx::PluginWindowHandle output_window_handle = handle_;
#endif  // defined(OS_WIN)

  // Initialize the CommandBufferService and GPUProcessor.
  if (command_buffer_->Initialize(size)) {
    Buffer buffer = command_buffer_->GetRingBuffer();
    if (buffer.shared_memory) {
      gpu::GPUProcessor* parent_processor =
          parent_ ? parent_->processor_.get() : NULL;
      processor_.reset(new gpu::GPUProcessor(command_buffer_.get(), NULL));
      if (processor_->Initialize(
          output_window_handle,
          initial_size_,
          allowed_extensions_.c_str(),
          requested_attribs_,
          parent_processor,
          parent_texture_id_)) {
        command_buffer_->SetPutOffsetChangeCallback(
            NewCallback(processor_.get(),
                        &gpu::GPUProcessor::ProcessCommands));
        processor_->SetSwapBuffersCallback(
            NewCallback(this, &GpuCommandBufferStub::OnSwapBuffers));

        // Assume service is responsible for duplicating the handle from the
        // calling process.
        buffer.shared_memory->ShareToProcess(channel_->renderer_process(),
                                             ring_buffer);
#if defined(OS_MACOSX)
        if (handle_) {
          // This context conceptually puts its output directly on the
          // screen, rendered by the accelerated plugin layer in
          // RenderWidgetHostViewMac. Set up a pathway to notify the
          // browser process when its contents change.
          processor_->SetSwapBuffersCallback(
              NewCallback(this,
                          &GpuCommandBufferStub::SwapBuffersCallback));
        }
#endif  // defined(OS_MACOSX)

        // Set up a pathway for resizing the output window or framebuffer at the
        // right time relative to other GL commands.
        processor_->SetResizeCallback(
            NewCallback(this, &GpuCommandBufferStub::ResizeCallback));
      } else {
        processor_.reset();
        command_buffer_.reset();
      }
    }
  }
}

void GpuCommandBufferStub::OnGetState(gpu::CommandBuffer::State* state) {
  *state = command_buffer_->GetState();
}

void GpuCommandBufferStub::OnAsyncGetState() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  Send(new GpuCommandBufferMsg_UpdateState(route_id_, state));
}

void GpuCommandBufferStub::OnFlush(int32 put_offset,
                                   gpu::CommandBuffer::State* state) {
#if defined(OS_MACOSX)
  // See comment in |DidDestroySurface()| in gpu_processor_mac.cc.
  if (channel_->IsRenderViewGone(render_view_id_))
    processor_->DidDestroySurface();
#endif
  *state = command_buffer_->FlushSync(put_offset);
}

void GpuCommandBufferStub::OnAsyncFlush(int32 put_offset) {
  gpu::CommandBuffer::State state = command_buffer_->FlushSync(put_offset);
  Send(new GpuCommandBufferMsg_UpdateState(route_id_, state));
}

void GpuCommandBufferStub::OnCreateTransferBuffer(int32 size, int32* id) {
  *id = command_buffer_->CreateTransferBuffer(size);
}

void GpuCommandBufferStub::OnRegisterTransferBuffer(
    base::SharedMemoryHandle transfer_buffer,
    size_t size,
    int32* id) {
#if defined(OS_WIN)
  base::SharedMemory shared_memory(transfer_buffer,
                                   false,
                                   channel_->renderer_process());
#else
  base::SharedMemory shared_memory(transfer_buffer, false);
#endif
  *id = command_buffer_->RegisterTransferBuffer(&shared_memory, size);
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(int32 id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void GpuCommandBufferStub::OnGetTransferBuffer(
    int32 id,
    base::SharedMemoryHandle* transfer_buffer,
    uint32* size) {
  *transfer_buffer = base::SharedMemoryHandle();
  *size = 0;

  // Fail if the renderer process has not provided its process handle.
  if (!channel_->renderer_process())
    return;

  Buffer buffer = command_buffer_->GetTransferBuffer(id);
  if (buffer.shared_memory) {
    // Assume service is responsible for duplicating the handle to the calling
    // process.
    buffer.shared_memory->ShareToProcess(channel_->renderer_process(),
                                         transfer_buffer);
    *size = buffer.size;
  }
}

void GpuCommandBufferStub::OnResizeOffscreenFrameBuffer(const gfx::Size& size) {
  processor_->ResizeOffscreenFrameBuffer(size);
}

void GpuCommandBufferStub::OnSwapBuffers() {
  Send(new GpuCommandBufferMsg_SwapBuffers(route_id_));
}

#if defined(OS_MACOSX)
void GpuCommandBufferStub::OnSetWindowSize(const gfx::Size& size) {
  GpuThread* gpu_thread = channel_->gpu_thread();
  // Try using the IOSurface version first.
  uint64 new_backing_store = processor_->SetWindowSizeForIOSurface(size);
  if (new_backing_store) {
    GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params params;
    params.renderer_id = renderer_id_;
    params.render_view_id = render_view_id_;
    params.window = handle_;
    params.width = size.width();
    params.height = size.height();
    params.identifier = new_backing_store;
    gpu_thread->Send(new GpuHostMsg_AcceleratedSurfaceSetIOSurface(params));
  } else {
    // TODO(kbr): figure out what to do here. It wouldn't be difficult
    // to support the compositor on 10.5, but the performance would be
    // questionable.
    NOTREACHED();
  }
}

void GpuCommandBufferStub::SwapBuffersCallback() {
  OnSwapBuffers();
  GpuThread* gpu_thread = channel_->gpu_thread();
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.window = handle_;
  params.surface_id = processor_->GetSurfaceId();
  params.route_id = route_id();
  params.swap_buffers_count = processor_->swap_buffers_count();
  gpu_thread->Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
}

void GpuCommandBufferStub::AcceleratedSurfaceBuffersSwapped(
    uint64 swap_buffers_count) {
  processor_->set_acknowledged_swap_buffers_count(swap_buffers_count);
  // Wake up the GpuProcessor to start doing work again.
  processor_->ScheduleProcessCommands();
}
#endif  // defined(OS_MACOSX)

void GpuCommandBufferStub::ResizeCallback(gfx::Size size) {
  if (handle_ == gfx::kNullPluginWindow) {
    processor_->decoder()->ResizeOffscreenFrameBuffer(size);
    processor_->decoder()->UpdateOffscreenFrameBufferSize();
  } else {
#if defined(OS_LINUX) && !defined(TOUCH_UI)
    GpuThread* gpu_thread = channel_->gpu_thread();
    bool result = false;
    gpu_thread->Send(
        new GpuHostMsg_ResizeXID(handle_, size, &result));
#elif defined(OS_WIN)
    HWND hwnd = static_cast<HWND>(compositor_window_);
    UINT swp_flags = SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOCOPYBITS |
      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE;
    SetWindowPos(hwnd, NULL, 0, 0, size.width(), size.height(), swp_flags);
#endif  // defined(OS_LINUX)
  }
}

#endif  // defined(ENABLE_GPU)
