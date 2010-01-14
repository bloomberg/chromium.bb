// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/main_function_params.h"
#include "chrome/gpu/gpu_process.h"
#include "chrome/gpu/gpu_thread.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_GpuMain").c_str());

#if defined(OS_WIN)
  win_util::ScopedCOMInitializer com_initializer;
#endif

  GpuProcess gpu_process;
  gpu_process.set_main_thread(new GpuThread());

  main_message_loop.Run();

  return 0;
}

