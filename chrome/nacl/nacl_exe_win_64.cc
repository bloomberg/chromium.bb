// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/app/chrome_breakpad_client.h"
#include "components/breakpad/app/breakpad_win.h"
#include "components/nacl/loader/nacl_helper_win_64.h"

namespace {

base::LazyInstance<chrome::ChromeBreakpadClient>::Leaky
    g_chrome_breakpad_client = LAZY_INSTANCE_INITIALIZER;

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);

  breakpad::SetBreakpadClient(g_chrome_breakpad_client.Pointer());
  breakpad::InitCrashReporter();

  return nacl::NaClWin64Main();
}
