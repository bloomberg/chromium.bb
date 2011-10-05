// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_main.h"

#include "content/shell/shell_main_delegate.h"
#include "sandbox/src/sandbox_types.h"

#if defined(OS_WIN)
#include "content/app/startup_helper_win.h"
#endif

#if defined(OS_WIN)

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  ShellMainDelegate delegate;
  return content::ContentMain(instance, &sandbox_info, &delegate);
}
#endif

#if defined(OS_POSIX)

#if defined(OS_MACOSX)
__attribute__((visibility("default")))
int main(int argc, const char* argv[]) {
#else
int main(int argc, const char** argv) {
#endif  // OS_MACOSX

  ShellMainDelegate delegate;
  return content::ContentMain(argc, argv, &delegate);
}

#endif  // OS_POSIX
