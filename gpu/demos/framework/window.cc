// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/window.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/demos/framework/demo_factory.h"

using gpu::Buffer;
using gpu::CommandBufferService;
using gpu::GPUProcessor;
using gpu::gles2::GLES2CmdHelper;
using gpu::gles2::GLES2Implementation;

// TODO(alokp): Make this class cross-platform. Investigate using SDL.
namespace {
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 512 * 1024;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
                            WPARAM w_param, LPARAM l_param) {
  LRESULT result = 0;
  switch (msg) {
    case WM_CLOSE:
      ::DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    case WM_ERASEBKGND:
      break;
    case WM_PAINT: {
      gpu::demos::Window* window = reinterpret_cast<gpu::demos::Window*>(
          GetWindowLongPtr(hwnd, GWL_USERDATA));
      if (window != NULL) window->OnPaint();
      ::ValidateRect(hwnd, NULL);
      break;
    }
    default:
      result = ::DefWindowProc(hwnd, msg, w_param, l_param);
      break;
  }
  return result;
}

HWND CreateNativeWindow(const wchar_t* title, int width, int height,
                        LONG_PTR user_data) {
  WNDCLASS wnd_class = {0};
  HINSTANCE instance = GetModuleHandle(NULL);
  wnd_class.style = CS_OWNDC;
  wnd_class.lpfnWndProc = WindowProc;
  wnd_class.hInstance = instance;
  wnd_class.hbrBackground =
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wnd_class.lpszClassName = L"gpu_demo";
  if (!RegisterClass(&wnd_class)) return NULL;

  DWORD wnd_style = WS_VISIBLE | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION;
  RECT wnd_rect;
  wnd_rect.left = 0;
  wnd_rect.top = 0;
  wnd_rect.right = width;
  wnd_rect.bottom = height;
  AdjustWindowRect(&wnd_rect, wnd_style, FALSE);

  HWND hwnd = CreateWindow(
      wnd_class.lpszClassName,
      title,
      wnd_style,
      0,
      0,
      wnd_rect.right - wnd_rect.left,
      wnd_rect.bottom - wnd_rect.top,
      NULL,
      NULL,
      instance,
      NULL);
  if (hwnd == NULL) return NULL;

  ShowWindow(hwnd, SW_SHOWNORMAL);
  // Set this to the GWL_USERDATA so that it is available to WindowProc.
  SetWindowLongPtr(hwnd, GWL_USERDATA, user_data);

  return hwnd;
}

bool InitRenderContext(HWND hwnd) {
  CHECK(hwnd);

  scoped_ptr<CommandBufferService> command_buffer(new CommandBufferService);
  if (!command_buffer->Initialize(kCommandBufferSize)) {
    return false;
  }

  scoped_refptr<GPUProcessor> gpu_processor(
      new GPUProcessor(command_buffer.get()));
  if (!gpu_processor->Initialize(hwnd)) {
    return false;
  }

  command_buffer->SetPutOffsetChangeCallback(
      NewCallback(gpu_processor.get(), &GPUProcessor::ProcessCommands));

  GLES2CmdHelper* helper = new GLES2CmdHelper(command_buffer.get());
  if (!helper->Initialize()) {
    // TODO(alokp): cleanup.
    return false;
  }

  int32 transfer_buffer_id =
      command_buffer->CreateTransferBuffer(kTransferBufferSize);
  Buffer transfer_buffer =
      command_buffer->GetTransferBuffer(transfer_buffer_id);
  if (transfer_buffer.ptr == NULL) return false;

  gles2::Initialize();
  gles2::SetGLContext(new GLES2Implementation(helper,
                                              transfer_buffer.size,
                                              transfer_buffer.ptr,
                                              transfer_buffer_id));
  return command_buffer.release() != NULL;
}
}  // namespace.

namespace gpu {
namespace demos {

Window::Window()
  : window_handle_(NULL),
    demo_(CreateDemo()) {
}

Window::~Window() {
}

bool Window::Init(int width, int height) {
  window_handle_ = CreateNativeWindow(demo_->Title(), width, height,
                                      reinterpret_cast<LONG_PTR>(this));
  if (window_handle_ == NULL) return false;
  if (!InitRenderContext(window_handle_)) return false;

  demo_->InitWindowSize(width, height);
  return demo_->InitGL();
}

void Window::MainLoop() {
  MSG msg;
  bool done = false;
  while (!done) {
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) done = true;
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
    // Message queue is empty and application has not quit yet - keep painting.
    if (!done) ::UpdateWindow(window_handle_);
  }
}

void Window::OnPaint() {
  demo_->Draw();
  ::gles2::GetGLContext()->SwapBuffers();
}

}  // namespace demos
}  // namespace gpu
