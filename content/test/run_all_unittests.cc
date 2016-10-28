// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"
#include "mojo/edk/test/scoped_ipc_support.h"

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(
      new content::ContentTestSuite(argc, argv));
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  std::unique_ptr<mojo::edk::test::ScopedIPCSupport> ipc_support;
  ipc_support.reset(
      new mojo::edk::test::ScopedIPCSupport(test_io_thread.task_runner()));

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&content::UnitTestTestSuite::Run,
                             base::Unretained(&test_suite)));
}
