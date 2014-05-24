// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/multiprocess_test_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "mojo/embedder/platform_channel_pair.h"

namespace mojo {
namespace test {

MultiprocessTestHelper::MultiprocessTestHelper()
    : test_child_handle_(base::kNullProcessHandle) {
  platform_channel_pair_.reset(new embedder::PlatformChannelPair());
  server_platform_handle = platform_channel_pair_->PassServerHandle();
}

MultiprocessTestHelper::~MultiprocessTestHelper() {
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);
  server_platform_handle.reset();
  platform_channel_pair_.reset();
}

void MultiprocessTestHelper::StartChild(const std::string& test_child_name) {
  CHECK(platform_channel_pair_.get());
  CHECK(!test_child_name.empty());
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);

  std::string test_child_main = test_child_name + "TestChildMain";

  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  embedder::HandlePassingInformation handle_passing_info;
  platform_channel_pair_->PrepareToPassClientHandleToChildProcess(
      &command_line, &handle_passing_info);

  base::LaunchOptions options;
#if defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#elif defined(OS_WIN)
  options.start_hidden = true;
  options.handles_to_inherit = &handle_passing_info;
#else
#error "Not supported yet."
#endif

  test_child_handle_ =
      base::SpawnMultiProcessTestChild(test_child_main, command_line, options);
  platform_channel_pair_->ChildProcessLaunched();

  CHECK_NE(test_child_handle_, base::kNullProcessHandle);
}

int MultiprocessTestHelper::WaitForChildShutdown() {
  CHECK_NE(test_child_handle_, base::kNullProcessHandle);

  int rv = -1;
  CHECK(base::WaitForExitCodeWithTimeout(
      test_child_handle_, &rv, TestTimeouts::action_timeout()));
  base::CloseProcessHandle(test_child_handle_);
  test_child_handle_ = base::kNullProcessHandle;
  return rv;
}

bool MultiprocessTestHelper::WaitForChildTestShutdown() {
  return WaitForChildShutdown() == 0;
}

// static
void MultiprocessTestHelper::ChildSetup() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());
  client_platform_handle =
      embedder::PlatformChannelPair::PassClientHandleFromParentProcess(
          *base::CommandLine::ForCurrentProcess());
}

// static
embedder::ScopedPlatformHandle MultiprocessTestHelper::client_platform_handle;

}  // namespace test
}  // namespace mojo
