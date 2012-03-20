// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metro_driver_win.h"

namespace {
// The windows 8 metro driver dll name and entry point.
const char kMetroDriverDll[] = "metro_driver.dll";
// This environment variable controls the loading of the metro driver DLL.
const char* kMetroModeEnvVar = "CHROME_METRO_DLL";

typedef int (*InitMetro)(LPTHREAD_START_ROUTINE thread_proc, void* context);

struct Context {
  MetroDriver::MainFn fn;
  HINSTANCE instance;
};

DWORD WINAPI MainThread(void* param) {
  Context* context = reinterpret_cast<Context*>(param);
  int rv = context->fn(context->instance);
  delete context;
  return rv;
}

}  // namespace

MetroDriver::MetroDriver() : init_metro_fn_(NULL) {
  if (0 != ::GetEnvironmentVariableA(kMetroModeEnvVar, NULL, 0))
    return;
  // We haven't tried to load the metro driver, this probably means we are the
  // browser. Find it or not we set the environment variable because we don't
  // want to keep trying in the child processes.
  HMODULE metro_dll = ::LoadLibraryA(kMetroDriverDll);
  ::SetEnvironmentVariableA(kMetroModeEnvVar, metro_dll ? "1" : "0");
  if (!metro_dll)
    return;
  init_metro_fn_ =
      ::GetProcAddress(::GetModuleHandleA(kMetroDriverDll), "InitMetro");
}

int MetroDriver::RunInMetro(HINSTANCE instance, MainFn main_fn) {
  Context* context = new Context;
  context->fn = main_fn;
  context->instance = instance;

  return reinterpret_cast<InitMetro>(init_metro_fn_)(&MainThread, context);
}
