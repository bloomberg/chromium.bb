// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "mojo/runner/child_process.h"
#include "mojo/runner/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/edk/embedder/test_embedder.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kChildProcess)) {
    base::AtExitManager at_exit;

    return mojo::runner::ChildProcessMain();
  }

  base::TestSuite test_suite(argc, argv);

  mojo::embedder::test::InitWithSimplePlatformSupport();

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
