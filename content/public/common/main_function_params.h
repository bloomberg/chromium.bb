// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wrapper to the parameter list for the "main" entry points (browser, renderer,
// plugin) to shield the call sites from the differences between platforms
// (e.g., POSIX doesn't need to pass any sandbox information).

#ifndef CONTENT_PUBLIC_COMMON_MAIN_FUNCTION_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_MAIN_FUNCTION_PARAMS_H_

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "build/build_config.h"

#if defined(OS_WIN)
namespace sandbox {
struct SandboxInterfaceInfo;
}
#elif defined(OS_MACOSX)
namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace content {

struct MainFunctionParams {
  explicit MainFunctionParams(const base::CommandLine& cl)
      : command_line(cl),
#if defined(OS_WIN)
        sandbox_info(NULL),
#elif defined(OS_MACOSX)
        autorelease_pool(NULL),
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
        zygote_child(false),
#endif
        ui_task(NULL) {
  }

  const base::CommandLine& command_line;

#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo* sandbox_info;
#elif defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool* autorelease_pool;
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  bool zygote_child;
#endif

#if defined(USE_AURA)
  aura::Env::Mode env_mode = aura::Env::Mode::LOCAL;
#endif

  // Whether DiscardableSharedMemoryManager should be created.
  bool create_discardable_memory = true;

  // Used by InProcessBrowserTest. If non-null BrowserMain schedules this
  // task to run on the MessageLoop and BrowserInit is not invoked.
  base::Closure* ui_task;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MAIN_FUNCTION_PARAMS_H_
