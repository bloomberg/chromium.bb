// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main.h"

#include "base/memory/scoped_ptr.h"
#include "content/public/app/content_main_runner.h"

namespace content {

#if defined(OS_WIN)
int ContentMain(HINSTANCE instance,
                sandbox::SandboxInterfaceInfo* sandbox_info,
                ContentMainDelegate* delegate) {
#else
int ContentMain(int argc,
                const char** argv,
                ContentMainDelegate* delegate) {
#endif  // OS_WIN

  scoped_ptr<ContentMainRunner> main_runner(ContentMainRunner::Create());

  int exit_code;

#if defined(OS_WIN)
  exit_code = main_runner->Initialize(instance, sandbox_info, delegate);
#else
  exit_code = main_runner->Initialize(argc, argv, delegate);
#endif  // OS_WIN

  if (exit_code >= 0)
    return exit_code;

  exit_code = main_runner->Run();

  main_runner->Shutdown();

  return exit_code;
}

}  // namespace content
