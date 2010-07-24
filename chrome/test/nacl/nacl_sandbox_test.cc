// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_sandbox_test.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_switches.h"

namespace {

// Base url is specified in nacl_test.
const FilePath::CharType kSrpcHwHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_hw.html");

}  // anonymous namespace

NaClSandboxTest::NaClSandboxTest() : NaClTest() {
  // Append the --test-nacl-sandbox=$TESTDLL flag before launching.
  FilePath dylibDir;
  PathService::Get(base::DIR_EXE, &dylibDir);
#if defined(OS_MACOSX)
  dylibDir = dylibDir.AppendASCII("libnacl_security_tests.dylib");
  launch_arguments_.AppendSwitchWithValue(switches::kTestNaClSandbox,
                                          dylibDir.value());
#elif defined(OS_WIN)
  // Let the NaCl process detect if it is 64-bit or not and hack on
  // the appropriate suffix to this dll.
  dylibDir = dylibDir.AppendASCII("nacl_security_tests");
  launch_arguments_.AppendSwitchWithValue(switches::kTestNaClSandbox,
                                          dylibDir.value());
#elif defined(OS_LINUX)
  // We currently do not test the Chrome Linux SUID or seccomp sandboxes.
#endif
}

NaClSandboxTest::~NaClSandboxTest() {}

// http://crbug.com/50121
TEST_F(NaClSandboxTest, FLAKY_NaClOuterSBTest) {
  // Load a helloworld .nexe to trigger the nacl loader test.
  FilePath test_file(kSrpcHwHtmlFileName);
  RunTest(test_file, action_max_timeout_ms());
}
