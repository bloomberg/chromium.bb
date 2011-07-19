// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl_security_tests/nacl_security_tests_posix.h"
#include <string>
#include "chrome/test/nacl_security_tests/commands_posix.h"

#define RETURN_IF_NOT_DENIED(x) \
  if (sandbox::SBOX_TEST_DENIED != x) { \
    return false; \
  }

// Runs the security tests of sandbox for the nacl loader process.
extern "C" bool RunNaClLoaderTests(void) {
  // Need to check if the system supports CLONE_NEWPID before testing
  // the filesystem accesses (otherwise the sandbox is not enabled).
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile("/etc"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile("/tmp"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile("$HOME"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("/etc"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("/etc/passwd"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("/bin"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("/usr/bin"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("/usr/bin/bash"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("/usr/bin/login"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("/usr/sbin"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile("$HOME"));

  // Linux (suid) sandbox doesn't block connect, etc...
  RETURN_IF_NOT_DENIED(sandbox::TestCreateProcess("/usr/bin/env"));
  RETURN_IF_NOT_DENIED(sandbox::TestConnect("www.archive.org"));

  return true;
}

