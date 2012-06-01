// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metro_driver_win.h"

#include <string.h>

#include "chrome/app/client_util.h"
#include "chrome/common/chrome_constants.h"

namespace {
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
  // The metro activation always has the |ServerName| parameter. If we dont
  // see it, we are being launched in desktop mode.
  if (!wcsstr(::GetCommandLineW(), L" -ServerName:DefaultBrowserServer")) {
    ::SetEnvironmentVariableA(kMetroModeEnvVar, "0");
    return;
  }
  // We haven't tried to load the metro driver, this probably means we are the
  // browser. Find it or not we set the environment variable because we don't
  // want to keep trying in the child processes.
  HMODULE metro_dll = ::LoadLibraryW(chrome::kMetroDriverDll);
  if (!metro_dll) {
    // It is not next to the build output, so this must be an actual deployment
    // and in that case we need the mainloader to find the current version
    // directory.
    MainDllLoader* loader = MakeMainDllLoader();
    std::wstring version = loader->GetVersion();
    delete loader;
    if (!version.empty()) {
      std::wstring exe_path(GetExecutablePath());
      exe_path.append(version).append(L"\\").append(chrome::kMetroDriverDll);
      metro_dll = ::LoadLibraryW(exe_path.c_str());
    }
  }
  // We set the environment variable always, so we don't keep trying in
  // the child processes.
  ::SetEnvironmentVariableA(kMetroModeEnvVar, metro_dll ? "1" : "0");
  if (!metro_dll)
    return;
  init_metro_fn_ = ::GetProcAddress(metro_dll, "InitMetro");
}

int MetroDriver::RunInMetro(HINSTANCE instance, MainFn main_fn) {
  Context* context = new Context;
  context->fn = main_fn;
  context->instance = instance;

  return reinterpret_cast<InitMetro>(init_metro_fn_)(&MainThread, context);
}
