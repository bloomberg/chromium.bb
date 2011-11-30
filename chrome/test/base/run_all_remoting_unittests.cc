// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A basic testrunner that supports JavaScript unittests.
// This lives in src/chrome/test/base so that it can include chrome_paths.h
// (required for JS unittests) without updating the DEPS file for each
// subproject.


#include "base/test/test_suite.h"
#include "chrome/common/chrome_paths.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // This is required for the JavaScript unittests.
  chrome::RegisterPathProvider();

  return test_suite.Run();
}
