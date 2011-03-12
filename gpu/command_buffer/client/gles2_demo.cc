// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a standalone executable demo of using GLES2 over
// command buffers. This code is temporary as the setup that happens
// here is in a state of flux. Eventually there will be several versions of
// setup, one for trusted code, one for pepper, one for NaCl, and maybe others.

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include "app/gfx/gl/gl_context.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/scoped_ptr.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/gles2_demo_c.h"
#include "gpu/command_buffer/client/gles2_demo_cc.h"

using base::SharedMemory;
using gpu::Buffer;
using gpu::GPUProcessor;
using gpu::CommandBufferService;
using gpu::gles2::GLES2CmdHelper;
using gpu::gles2::GLES2Implementation;

#if defined(OS_WIN)
HINSTANCE g_instance;
#endif

class GLES2Demo {
 public:
  GLES2Demo();

  bool GLES2Demo::Setup(void* hwnd, int32 size);

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Demo);
};

GLES2Demo::GLES2Demo() {
}

bool GLES2Demo::Setup(void* hwnd, int32 size) {
  scoped_ptr<CommandBufferService> command_buffer(new CommandBufferService);
  if (!command_buffer->Initialize(size))
    return NULL;

  GPUProcessor* gpu_processor = new GPUProcessor(command_buffer.get(), NULL);
  if (!gpu_processor->Initialize(reinterpret_cast<HWND>(hwnd),
                                 gfx::Size(),
                                 NULL,
                                 std::vector<int32>(),
                                 NULL,
                                 0)) {
    return NULL;
  }

  command_buffer->SetPutOffsetChangeCallback(
      NewCallback(gpu_processor, &GPUProcessor::ProcessCommands));

  GLES2CmdHelper* helper = new GLES2CmdHelper(command_buffer.get());
  if (!helper->Initialize(size)) {
    // TODO(gman): cleanup.
    return false;
  }

  size_t transfer_buffer_size = 512 * 1024;
  int32 transfer_buffer_id =
      command_buffer->CreateTransferBuffer(transfer_buffer_size);
  Buffer transfer_buffer =
      command_buffer->GetTransferBuffer(transfer_buffer_id);
  if (!transfer_buffer.ptr)
    return false;


  gles2::Initialize();
  gles2::SetGLContext(new GLES2Implementation(helper,
                                              transfer_buffer.size,
                                              transfer_buffer.ptr,
                                              transfer_buffer_id,
                                              false));

  GLFromCPPInit();

  return command_buffer.release() != NULL;
}

#if defined(OS_WIN)
LRESULT CALLBACK WindowProc(
    HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  switch (msg) {
    case WM_CLOSE:
      DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_PAINT: {
      GLFromCPPDraw();
      GLFromCDraw();
      // TODO(gman): Not sure how SwapBuffer should be exposed.
      gles2::GetGLContext()->SwapBuffers();
      break;
    }
    default:
      return ::DefWindowProc(hwnd, msg, w_param, l_param);
  }
  return 0;
}

void ProcessMessages(void* in_hwnd) {
  HWND hwnd = reinterpret_cast<HWND>(in_hwnd);
  MSG msg;

  bool done = false;
  while (!done) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        done = true;
      }
      // dispatch the message
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (!done) {
      InvalidateRect(hwnd, NULL, TRUE);
    }
  }
}

#endif

void* SetupWindow() {
#if defined(OS_WIN)
  WNDCLASSEX wc = {0};
  wc.lpszClassName = L"MY_WINDOWS_CLASS";
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = ::WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = g_instance;
  wc.hIcon = ::LoadIcon(g_instance, IDI_APPLICATION);
  wc.hIconSm = NULL;
  wc.hCursor = ::LoadCursor(g_instance, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH));
  wc.lpszMenuName = NULL;

  if (!::RegisterClassEx(&wc))
    return false;

  // Leaving this window onscreen leads to a redraw error which makes it
  // a hassle to debug tests in an IDE, so we place the window somewhere that
  // won't happen.
  HWND hwnd = ::CreateWindowExW(
      NULL,
      wc.lpszClassName,
      L"",
      WS_OVERLAPPEDWINDOW,
      10,
      0,
      512,
      512,
      0,
      0,
      g_instance,
      0);

  if (hwnd == NULL) {
    return false;
  }

  ::ShowWindow(hwnd, SW_SHOWNORMAL);


  return hwnd;
#else
#error Need code.
#endif
}

#if defined(OS_WIN)
int WINAPI WinMain(HINSTANCE instance,
                   HINSTANCE prev_instance,
                   LPSTR command_line,
                   int command_show) {
  g_instance = instance;
  int argc = 1;
  const char* const argv[] = {
    "gles2_demo.exe"
  };

#else
int main(int argc, char** argv) {
#endif
  CommandLine::Init(argc, argv);

  const int32 kCommandBufferSize = 1024 * 1024;

  base::AtExitManager at_exit_manager;
  MessageLoopForUI message_loop;

  gfx::GLContext::InitializeOneOff();

  GLES2Demo* demo = new GLES2Demo();

  void* hwnd = SetupWindow();
  if (!hwnd) {
    ::fprintf(stdout, "Could not setup window.\n");
    return EXIT_FAILURE;
  }

  demo->Setup(hwnd, kCommandBufferSize);

  ProcessMessages(hwnd);

  return EXIT_SUCCESS;
}
