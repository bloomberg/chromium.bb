// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/plugin/command_buffer_stub.h"

namespace {
const wchar_t* kPreviousWndProcProperty = L"CommandBufferStubPrevWndProc";
const wchar_t* kCommandBufferStubProperty = L"CommandBufferStub";

// Message handler for the GPU plugin's child window. Used to intercept
// WM_PAINT events and forward repaint notifications to the client.
LRESULT WINAPI WndProc(HWND handle,
                       UINT message,
                       WPARAM w_param,
                       LPARAM l_param) {
  WNDPROC previous_wnd_proc = reinterpret_cast<WNDPROC>(
      ::GetProp(handle, kPreviousWndProcProperty));
  CommandBufferStub* stub = reinterpret_cast<CommandBufferStub*>(
      ::GetProp(handle, kCommandBufferStubProperty));

  switch (message) {
    case WM_ERASEBKGND:
      // Do not clear background. Avoids flickering.
      return 1;
    case WM_PAINT:
      // Validate the whole window to prevent another WM_PAINT message.
      ValidateRect(handle, NULL);

      // Notify client that the window is invalid and needs to be repainted.
      stub->NotifyRepaint();

      return 1;
    default:
      return CallWindowProc(previous_wnd_proc,
                            handle,
                            message,
                            w_param,
                            l_param);
  }
}
}  // namespace anonymous

bool CommandBufferStub::InitializePlatformSpecific() {
  // Subclass window.
  WNDPROC previous_wnd_proc = reinterpret_cast<WNDPROC>(
      ::GetWindowLongPtr(window_, GWLP_WNDPROC));
  ::SetProp(window_,
            kPreviousWndProcProperty,
            reinterpret_cast<HANDLE>(previous_wnd_proc));
  ::SetWindowLongPtr(window_,
                     GWLP_WNDPROC,
                     reinterpret_cast<LONG_PTR>(WndProc));

  // Record pointer to this in window.
  ::SetProp(window_,
            kCommandBufferStubProperty,
            reinterpret_cast<HANDLE>(this));

  return true;
}

void CommandBufferStub::DestroyPlatformSpecific() {
  // Restore window.
  WNDPROC previous_wnd_proc = reinterpret_cast<WNDPROC>(
      ::GetProp(window_, kPreviousWndProcProperty));
  ::SetWindowLongPtr(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(
      previous_wnd_proc));
  ::RemoveProp(window_, kPreviousWndProcProperty);
  ::RemoveProp(window_, kCommandBufferStubProperty);
}
