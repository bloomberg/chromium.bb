// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/platform_thread.h"
#include "base/perf_test_suite.h"
#include "base/scoped_ptr.h"
#include "chrome/common/chrome_paths.h"
#include "chrome_frame/test_utils.h"

int main(int argc, char **argv) {
  PerfTestSuite perf_suite(argc, argv);
  chrome::RegisterPathProvider();
  PlatformThread::SetName("ChromeFrame perf tests");
  // Use ctor/raii to register the local Chrome Frame dll.
  scoped_ptr<ScopedChromeFrameRegistrar> registrar(new ScopedChromeFrameRegistrar);
  return perf_suite.Run();
}
