// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "chrome/common/chrome_paths.h"

int main(int argc, char** argv) {
  TestSuite test_suite(argc, argv);

  // Register Chrome Path provider so that we can get test data dir.
  chrome::RegisterPathProvider();

  return test_suite.Run();
}
