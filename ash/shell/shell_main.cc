// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_main_delegate.h"
#include "content/public/app/content_main.h"
#include "sandbox/win/src/sandbox_types.h"
#include "ui/views/corewm/transient_window_stacking_client.h"

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#endif

namespace {

void CommonInit() {
  // SetWindowStackingClient() takes ownership of TransientWindowStackingClient.
  aura::client::SetWindowStackingClient(
      new views::corewm::TransientWindowStackingClient);
}

}  // namespace

#if defined(OS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  ash::shell::ShellMainDelegate delegate;
  CommonInit();
  return content::ContentMain(instance, &sandbox_info, &delegate);
}
#else
int main(int argc, const char** argv) {
  ash::shell::ShellMainDelegate delegate;
  CommonInit();
  return content::ContentMain(argc, argv, &delegate);
}
#endif
