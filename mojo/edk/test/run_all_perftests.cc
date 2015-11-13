// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/perf_test_suite.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/test_support_impl.h"
#include "mojo/public/tests/test_support_private.h"

int main(int argc, char** argv) {
  mojo::edk::Init();
  mojo::test::TestSupport::Init(new mojo::edk::test::TestSupportImpl());

  // TODO(use_chrome_edk): temporary to force new EDK.
  base::PerfTestSuite test(argc, argv);
  base::CommandLine::ForCurrentProcess()->AppendSwitch("--use-new-edk");

  return test.Run();
}
