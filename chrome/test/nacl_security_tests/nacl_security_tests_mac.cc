// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/test/nacl_security_tests/nacl_security_tests_posix.h"
#include <string>
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/test/nacl_security_tests/commands_posix.h"

__attribute__((constructor))
static void initializer(void) {
}

__attribute__((destructor))
static void finalizer(void) {
}

namespace {
void CheckDenied(sandbox::SboxTestResult x) {
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, x);
}
}  // anon namespace

extern "C"
__attribute__((visibility("default")))
bool RunNaClLoaderTests(void) {
  CheckDenied(sandbox::TestOpenReadFile("/etc"));
  CheckDenied(sandbox::TestOpenReadFile("/tmp"));
  CheckDenied(sandbox::TestOpenReadFile("$HOME"));
  CheckDenied(sandbox::TestOpenWriteFile("/etc"));
  CheckDenied(sandbox::TestOpenWriteFile("/etc/passwd"));
  CheckDenied(sandbox::TestOpenWriteFile("/bin"));
  CheckDenied(sandbox::TestOpenWriteFile("/usr/bin"));
  CheckDenied(sandbox::TestOpenWriteFile("/usr/bin/bash"));
  CheckDenied(sandbox::TestOpenWriteFile("/usr/bin/login"));
  CheckDenied(sandbox::TestOpenWriteFile("/usr/sbin"));
  CheckDenied(sandbox::TestOpenWriteFile("$HOME"));

  CheckDenied(sandbox::TestCreateProcess("/usr/bin/env"));
  CheckDenied(sandbox::TestConnect("www.archive.org"));

  return !(testing::Test::HasFatalFailure() ||
           testing::Test::HasNonfatalFailure());
}

