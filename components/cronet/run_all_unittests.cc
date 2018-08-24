// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
#if defined(CRONET_TESTS_IMPLEMENTATION) && defined(COMPONENT_BUILD)
  // In component builds cronet_tests and libcronet.so share various libraries,
  // so globals initialized by libcronet.so are visible to the TestSuite.
  test_suite.DisableCheckForLeakedGlobals();
#endif
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
