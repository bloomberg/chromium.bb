// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/profile_import/profile_import_thread.h"
#include "content/common/child_process.h"
#include "content/common/main_function_params.h"

// Mainline routine for running as the profile import process.
int ProfileImportMain(const MainFunctionParams& parameters) {
  // The main message loop of the profile import process.
  MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrProfileImportMain");

  ChildProcess profile_import_process;
  profile_import_process.set_main_thread(new ProfileImportThread());

  MessageLoop::current()->Run();

  return 0;
}
