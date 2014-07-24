// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main.h"
#include "extensions/shell/app/shell_main_delegate.h"

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(OS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
#else
int main(int argc, const char** argv) {
#endif
  extensions::ShellMainDelegate delegate;
  content::ContentMainParams params(&delegate);

#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  params.instance = instance;
  params.sandbox_info = &sandbox_info;
#else
  params.argc = argc;
  params.argv = argv;
#endif

  return content::ContentMain(params);
}
