// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"
#include "chrome/test/ui/ui_test_suite.h"

int main(int argc, char **argv) {
  base::PlatformThread::SetName("Tests_Main");
  return UITestSuite(argc, argv).Run();
}
