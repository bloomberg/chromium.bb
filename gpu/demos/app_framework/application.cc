// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/app_framework/application.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using gpu::Buffer;
using gpu::CommandBufferService;
using gpu::GPUProcessor;
using gpu::gles2::GLES2CmdHelper;
using gpu::gles2::GLES2Implementation;

// TODO(alokp): Implement it on mac and linux when gpu process is functional
// on these OS'.
#if defined(OS_WIN)
namespace {
static const int32 kCommandBufferSize = 1024 * 1024;
static const int32 kTransferBufferSize = 512 * 1024;

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
                                   WPARAM w_param, LPARAM l_param) {
  LRESULT result = 0;
  switch (msg) {
    case WM_CLOSE:
      ::DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    case WM_PAINT: {
      using gpu_demos::Application;
      Application* app = reinterpret_cast<Application*>(
          GetWindowLongPtr(hwnd, GWL_USERDATA));
      if (app != NULL) app->OnPaint();
      ::ValidateRect(hwnd, NULL);
      break;
    }
    default:
      result = ::DefWindowProc(hwnd, msg, w_param, l_param);
      break;
  }
  return result;
}
}  // namespace.

namespace gpu_demos {

Application::Application()
    : width_(512),
      height_(512),
      window_handle_(NULL) {
}

Application::~Application() {
}

void Application::MainLoop() {
  MSG msg;
  bool done = false;
  while (!done) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) done = true;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    // Message queue is empty and application has not quit yet - keep painting.
    if (!done) SendMessage(window_handle_, WM_PAINT, 0, 0);
  }
}

void Application::OnPaint() {
  float elapsed_sec = 0.0f;
  const base::Time current_time = base::Time::Now();
  if (!last_draw_time_.is_null()) {
    base::TimeDelta time_delta = current_time - last_draw_time_;
    elapsed_sec = static_cast<float>(time_delta.InSecondsF());
  }
  last_draw_time_ = current_time;

  Draw(elapsed_sec);
  gles2::GetGLContext()->SwapBuffers();
}

bool Application::InitRenderContext() {
  window_handle_ = CreateNativeWindow();
  if (window_handle_ == NULL) {
    return false;
  }

  scoped_ptr<CommandBufferService> command_buffer(new CommandBufferService);
  if (!command_buffer->Initialize(kCommandBufferSize)) {
    return false;
  }

  scoped_refptr<GPUProcessor> gpu_processor(
      new GPUProcessor(command_buffer.get()));
  if (!gpu_processor->Initialize(window_handle_)) {
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

  gles2::g_gl_impl = new GLES2Implementation(helper,
                                             transfer_buffer.size,
                                             transfer_buffer.ptr,
                                             transfer_buffer_id);

  return command_buffer.release() != NULL;
}

NativeWindowHandle Application::CreateNativeWindow() {
  WNDCLASS wnd_class = {0};
  HINSTANCE instance = GetModuleHandle(NULL);
  wnd_class.style = CS_OWNDC;
  wnd_class.lpfnWndProc = (WNDPROC)WindowProc;
  wnd_class.hInstance = instance;
  wnd_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wnd_class.lpszClassName = L"opengles2.0";
  if (!RegisterClass(&wnd_class)) return NULL;

  DWORD wnd_style = WS_VISIBLE | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION;
  RECT wnd_rect;
  wnd_rect.left = 0;
  wnd_rect.top = 0;
  wnd_rect.right = width_;
  wnd_rect.bottom = height_;
  AdjustWindowRect(&wnd_rect, wnd_style, FALSE);

  HWND hwnd = CreateWindow(
      wnd_class.lpszClassName,
      wnd_class.lpszClassName,
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
  SetWindowLongPtr(hwnd, GWL_USERDATA, (LONG_PTR)this);

  return hwnd;
}

}  // namespace gpu_demos
#endif  // defined(OS_WIN)
