// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_TEST_MULTIPROCESS_TEST_BASE_H_
#define MOJO_COMMON_TEST_MULTIPROCESS_TEST_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/process/process_handle.h"
#include "base/test/multiprocess_test.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "testing/multiprocess_func_list.h"

namespace mojo {

namespace embedder {
class PlatformChannelPair;
}

namespace test {

class MultiprocessTestBase : public base::MultiProcessTest {
 public:
  MultiprocessTestBase();
  virtual ~MultiprocessTestBase();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Start a child process and run the "main" function "named" |test_child_name|
  // declared using |MOJO_MULTIPROCESS_TEST_CHILD_MAIN()| (below).
  void StartChild(const std::string& test_child_name);
  // Wait for the child process to terminate.
  // Returns the exit code of the child process. Note that, though it's declared
  // to be an |int|, the exit code is subject to mangling by the OS. E.g., we
  // usually return -1 on error in the child (e.g., if |test_child_name| was not
  // found), but this is mangled to 255 on Linux. You should only rely on codes
  // 0-127 being preserved, and -1 being outside the range 0-127.
  int WaitForChildShutdown();

  // For use by |MOJO_MULTIPROCESS_TEST_CHILD_MAIN()| only:
  static void ChildSetup();

  // For use in the main process:
  embedder::ScopedPlatformHandle server_platform_handle;

  // For use (and only valid) in the child process:
  static embedder::ScopedPlatformHandle client_platform_handle;

 private:
  virtual CommandLine MakeCmdLine(const std::string& procname,
                                  bool debug_on_start) OVERRIDE;

  scoped_ptr<embedder::PlatformChannelPair> platform_channel_pair_;

  // Valid after |StartChild()| and before |WaitForChildShutdown()|.
  base::ProcessHandle test_child_handle_;

  DISALLOW_COPY_AND_ASSIGN(MultiprocessTestBase);
};

// Use this to declare the child process's "main()" function for tests using
// |MultiprocessTestBase|. It returns an |int|, which will be the process's exit
// code (but see the comment about |WaitForChildShutdown()|).
#define MOJO_MULTIPROCESS_TEST_CHILD_MAIN(test_child_name) \
    MULTIPROCESS_TEST_MAIN_WITH_SETUP( \
        test_child_name ## TestChildMain, \
        ::mojo::test::MultiprocessTestBase::ChildSetup)

}  // namespace test
}  // namespace mojo

#endif  // MOJO_COMMON_TEST_MULTIPROCESS_TEST_BASE_H_
