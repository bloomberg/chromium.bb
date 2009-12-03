// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>

#include "base/test/test_suite.h"
#include "chrome/common/chrome_paths.h"

int main(int argc, char** argv) {
  TestSuite test_suite(argc, argv);

  // Register Chrome Path provider so that we can get test data dir.
  chrome::RegisterPathProvider();

  if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK)
    return -1;

  int ret = test_suite.Run();
  CoUninitialize();
  return ret;
}
