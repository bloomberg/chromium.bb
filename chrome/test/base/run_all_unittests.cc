// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "content/public/test/unittest_test_suite.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

int main(int argc, char **argv) {
  content::UnitTestTestSuite test_suite(new ChromeUnitTestSuite(argc, argv));

  // TODO(use_chrome_edk): This flag will go away and be default behavior soon,
  // but we explicitly add it here for test coverage.
  base::CommandLine::ForCurrentProcess()->AppendSwitch("use-new-edk");

  mojo::embedder::Init();
  return base::LaunchUnitTests(
      argc, argv, base::Bind(&content::UnitTestTestSuite::Run,
                             base::Unretained(&test_suite)));
}
