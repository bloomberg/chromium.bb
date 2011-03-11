// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wrapper to the parameter list for the "main" entry points (browser, renderer,
// plugin) to shield the call sites from the differences between platforms
// (e.g., POSIX doesn't need to pass any sandbox information).

#ifndef CONTENT_COMMON_MAIN_FUNCTION_PARAMS_H_
#define CONTENT_COMMON_MAIN_FUNCTION_PARAMS_H_
#pragma once

#include "base/command_line.h"
#include "content/common/sandbox_init_wrapper.h"

namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}

class Task;

struct MainFunctionParams {
  MainFunctionParams(const CommandLine& cl, const SandboxInitWrapper& sb,
                     base::mac::ScopedNSAutoreleasePool* pool)
      : command_line_(cl), sandbox_info_(sb), autorelease_pool_(pool),
        ui_task(NULL) { }
  const CommandLine& command_line_;
  const SandboxInitWrapper& sandbox_info_;
  base::mac::ScopedNSAutoreleasePool* autorelease_pool_;
  // Used by InProcessBrowserTest. If non-null BrowserMain schedules this
  // task to run on the MessageLoop and BrowserInit is not invoked.
  Task* ui_task;
};

#endif  // CONTENT_COMMON_MAIN_FUNCTION_PARAMS_H_
