// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_suite.h"
#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"

int main(int argc, char** argv) {
  ash::test::AuraShellTestSuite test_suite(argc, argv);

  return base::LaunchUnitTestsSerially(
      argc,
      argv,
      base::Bind(&ash::test::AuraShellTestSuite::Run,
                 base::Unretained(&test_suite)));
}
