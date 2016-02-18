// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

int RunHelper(base::TestSuite* test_suite) {
  content::UnitTestTestSuite runner(test_suite);
  base::MessageLoopForIO message_loop;
  return runner.Run();
}

}  // namespace

// These tests needs to run against a proper GL environment, so we
// need to set it up before we can run the tests.
int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::TestSuite* suite = new content::ContentTestSuite(argc, argv);
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  return base::LaunchUnitTestsSerially(
    argc,
    argv,
    base::Bind(&RunHelper, base::Unretained(suite)));
}
