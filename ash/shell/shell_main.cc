// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main.h"
#include "sandbox/src/sandbox_types.h"
#include "ash/shell/content_client/shell_main_delegate.h"

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#endif

#if defined(OS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  ash::shell::ShellMainDelegate delegate;
  return content::ContentMain(instance, &sandbox_info, &delegate);
}
#else
int main(int argc, const char** argv) {
  ash::shell::ShellMainDelegate delegate;
  return content::ContentMain(argc, argv, &delegate);
}
#endif
