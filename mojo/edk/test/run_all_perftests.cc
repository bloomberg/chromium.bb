// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/perf_test_suite.h"
#include "base/test/test_io_thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/edk/test/scoped_ipc_support.h"
#include "mojo/edk/test/test_support_impl.h"
#include "mojo/public/tests/test_support_private.h"

int main(int argc, char** argv) {
  base::PerfTestSuite test(argc, argv);

  // TODO(use_chrome_edk): temporary to force new EDK.
  base::CommandLine::ForCurrentProcess()->AppendSwitch("--use-new-edk");

  mojo::edk::Init();
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  // Leak this because its destructor calls mojo::edk::ShutdownIPCSupport which
  // really does nothing in the new EDK but does depend on the current message
  // loop, which is destructed inside base::LaunchUnitTests.
  new mojo::edk::test::ScopedIPCSupport(test_io_thread.task_runner());
  mojo::test::TestSupport::Init(new mojo::edk::test::TestSupportImpl());

  return test.Run();
}
