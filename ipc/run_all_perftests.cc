// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied from mojo/core/test/run_all_perftests.cc.

#include "base/command_line.h"
#include "base/test/perf_test_suite.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/core/test/test_support_impl.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "mojo/core/embedder/default_mach_broker.h"
#endif

int main(int argc, char** argv) {
  base::PerfTestSuite test(argc, argv);

  mojo::core::Init();
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  mojo::core::ScopedIPCSupport ipc_support(
      test_io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  mojo::test::TestSupport::Init(new mojo::core::test::TestSupportImpl());
#if defined(OS_MACOSX) && !defined(OS_IOS)
  mojo::core::SetMachPortProvider(
      mojo::core::DefaultMachBroker::Get()->port_provider());
#endif

  return test.Run();
}
