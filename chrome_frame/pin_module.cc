// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/pin_module.h"

#include <windows.h>

#include "base/logging.h"
#include "chrome_frame/utils.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace chrome_frame {

namespace {

PinModuleCallbackFn g_pin_module_callback = NULL;

}  // namespace

void SetPinModuleCallback(PinModuleCallbackFn callback) {
  g_pin_module_callback = callback;
}

void PinModule() {
  static bool s_pinned = false;
  if (!s_pinned && !IsUnpinnedMode()) {
    wchar_t system_buffer[MAX_PATH];
    HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
    system_buffer[0] = 0;
    if (GetModuleFileName(this_module, system_buffer,
                          arraysize(system_buffer)) != 0) {
      HMODULE unused;
      if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN, system_buffer,
                             &unused)) {
        DPLOG(FATAL) << "Failed to pin module " << system_buffer;
      } else {
        s_pinned = true;
        if (g_pin_module_callback)
          g_pin_module_callback();
      }
    } else {
      DPLOG(FATAL) << "Could not get module path.";
    }
  }
}

}  // namespace chrome_frame
