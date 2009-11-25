// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/gpu_plugin/gpu_plugin_object.h"

namespace gpu_plugin {

namespace {
const LPCTSTR kPluginObjectProperty = TEXT("GPUPluginObject");
const LPCTSTR kOriginalWindowProc = TEXT("GPUPluginObjectOriginalWindowProc");

LRESULT CALLBACK WindowProc(HWND handle,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param) {
  return ::DefWindowProc(handle, message, w_param, l_param);
}
}  // namespace anonymous

NPError GPUPluginObject::PlatformSpecificSetWindow(NPWindow* new_window) {
  // Detach properties from old window and restore the original window proc.
  if (window_.window) {
    HWND handle = reinterpret_cast<HWND>(window_.window);
    ::RemoveProp(handle, kPluginObjectProperty);

    LONG original_window_proc = reinterpret_cast<LONG>(
        ::GetProp(handle, kOriginalWindowProc));
    ::SetWindowLong(handle, GWL_WNDPROC,
                    original_window_proc);
    ::RemoveProp(handle, kOriginalWindowProc);
  }

  // Attach properties to new window and set a new window proc.
  if (new_window->window) {
    HWND handle = reinterpret_cast<HWND>(new_window->window);
    ::SetProp(handle,
              kPluginObjectProperty,
              reinterpret_cast<HANDLE>(this));

    LONG original_window_proc = ::GetWindowLong(handle, GWL_WNDPROC);
    ::SetProp(handle,
              kOriginalWindowProc,
              reinterpret_cast<HANDLE>(original_window_proc));
    ::SetWindowLong(handle, GWL_WNDPROC,
                    reinterpret_cast<LONG>(WindowProc));

    status_ = kWaitingForOpenCommandBuffer;
  } else {
    status_ = kWaitingForSetWindow;
    if (processor_) {
      processor_->Destroy();
    }
  }

  return NPERR_NO_ERROR;
}

}  // namespace gpu_plugin
