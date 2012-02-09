// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_delegate.h"

#include "content/public/app/content_main.h"

#if defined(OS_WIN)
#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info);
}
#elif defined(OS_POSIX)
extern "C" {
__attribute__((visibility("default")))
int ChromeMain(int argc, const char** argv);
}
#endif

#if defined(OS_WIN)
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info) {
  ChromeMainDelegate chrome_main_delegate;
  return content::ContentMain(instance, sandbox_info, &chrome_main_delegate);
#elif defined(OS_POSIX)
int ChromeMain(int argc, const char** argv) {
  ChromeMainDelegate chrome_main_delegate;
  return content::ContentMain(argc, argv, &chrome_main_delegate);
#endif
}
