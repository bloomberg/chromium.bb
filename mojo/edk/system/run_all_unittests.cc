// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
// Silence death test thread warnings on Linux. We can afford to run our death
// tests a little more slowly (< 10 ms per death test on a Z620).
// On android, we need to run in the default mode, as the threadsafe mode
// relies on execve which is not available.
#if !defined(OS_ANDROID)
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
#endif
  base::TestSuite test_suite(argc, argv);

  // Must be run before mojo::edk::Init.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          mojo::edk::test::kBrokerHandleSwitch)) {
    mojo::edk::PreInitializeChildProcess();
  }

  // TODO(use_chrome_edk): temporary to force new EDK.
  base::CommandLine::ForCurrentProcess()->AppendSwitch("--use-new-edk");

  mojo::edk::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
