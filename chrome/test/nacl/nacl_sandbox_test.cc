// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_sandbox_test.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "chrome/common/chrome_switches.h"

namespace {

// Base url is specified in nacl_test.
const FilePath::CharType kSrpcHwHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_hw.html");

}  // namespace

NaClSandboxTest::NaClSandboxTest() {
  // Append the --test-nacl-sandbox=$TESTDLL flag before launching.
  FilePath dylib_dir;
  PathService::Get(base::DIR_EXE, &dylib_dir);
#if defined(OS_MACOSX)
  dylib_dir = dylib_dir.AppendASCII("libnacl_security_tests.dylib");
  launch_arguments_.AppendSwitchPath(switches::kTestNaClSandbox, dylib_dir);
#elif defined(OS_WIN)
  // Let the NaCl process detect if it is 64-bit or not and hack on
  // the appropriate suffix to this dll.
  dylib_dir = dylib_dir.AppendASCII("nacl_security_tests");
  launch_arguments_.AppendSwitchPath(switches::kTestNaClSandbox, dylib_dir);
#elif defined(OS_LINUX)
  // We currently do not test the Chrome Linux SUID or seccomp sandboxes.
#endif
}

NaClSandboxTest::~NaClSandboxTest() {
}

TEST_F(NaClSandboxTest, DISABLED_NaClOuterSBTest) {
  // Load a helloworld .nexe to trigger the nacl loader test.
  FilePath test_file(kSrpcHwHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}
