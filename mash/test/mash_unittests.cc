// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "mash/test/mash_test_suite.h"

int MasterProcessMain(int argc, char** argv) {
  mash::test::MashTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(argc, argv,
                               base::Bind(&mash::test::MashTestSuite::Run,
                                          base::Unretained(&test_suite)));
}
