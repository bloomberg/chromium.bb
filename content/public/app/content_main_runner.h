// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_

#include <string>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

class ContentMainDelegate;

// This class is responsible for content initialization, running and shutdown.
class ContentMainRunner {
 public:
  virtual ~ContentMainRunner() {}

  // Create a new ContentMainRunner object.
  static ContentMainRunner* Create();

  // Initialize all necessary content state.
#if defined(OS_WIN)
  // The |sandbox_info| and |delegate| objects must outlive this class.
  // |sandbox_info| should be initialized using InitializeSandboxInfo from
  // content_main_win.h.
  virtual int Initialize(HINSTANCE instance,
                         sandbox::SandboxInterfaceInfo* sandbox_info,
                         ContentMainDelegate* delegate) = 0;
#else
  // The |delegate| object must outlive this class.
  virtual int Initialize(int argc,
                         const char** argv,
                         ContentMainDelegate* delegate) = 0;
#endif

  // Perform the default run logic.
  virtual int Run() = 0;

  // Shut down the content state.
  virtual void Shutdown() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_
