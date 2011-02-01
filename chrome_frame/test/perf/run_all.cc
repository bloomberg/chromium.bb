// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/perf_test_suite.h"
#include "base/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"

void PureCall() {
  __debugbreak();
}

int main(int argc, char **argv) {
  base::PerfTestSuite perf_suite(argc, argv);
  chrome::RegisterPathProvider();
  base::PlatformThread::SetName("ChromeFrame perf tests");

  _set_purecall_handler(PureCall);

  SetConfigBool(kChromeFrameHeadlessMode, true);
  SetConfigBool(kChromeFrameUnpinnedMode, true);

  base::ProcessHandle crash_service = chrome_frame_test::StartCrashService();

  // Use ctor/raii to register the local Chrome Frame dll.
  scoped_ptr<ScopedChromeFrameRegistrar> registrar(
      new ScopedChromeFrameRegistrar(
          ScopedChromeFrameRegistrar::SYSTEM_LEVEL));
  int ret = perf_suite.Run();

  DeleteConfigValue(kChromeFrameHeadlessMode);
  DeleteConfigValue(kChromeFrameUnpinnedMode);

  if (crash_service)
    base::KillProcess(crash_service, 0, false);
  return ret;
}
