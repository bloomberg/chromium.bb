// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/multiprocess_test_base.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "mojo/system/embedder/platform_channel_pair.h"

namespace mojo {
namespace test {

MultiprocessTestBase::MultiprocessTestBase()
    : test_child_handle_(base::kNullProcessHandle) {
}

MultiprocessTestBase::~MultiprocessTestBase() {
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);
}

void MultiprocessTestBase::SetUp() {
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);

  MultiProcessTest::SetUp();

  platform_channel_pair_.reset(new embedder::PlatformChannelPair());
  server_platform_handle = platform_channel_pair_->PassServerHandle();
}

void MultiprocessTestBase::TearDown() {
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);

  server_platform_handle.reset();
  platform_channel_pair_.reset();

  MultiProcessTest::TearDown();
}

void MultiprocessTestBase::StartChild(const std::string& test_child_name) {
  CHECK(platform_channel_pair_.get());
  CHECK(!test_child_name.empty());
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);

  std::string test_child_main = test_child_name + "TestChildMain";

  CommandLine unused(CommandLine::NO_PROGRAM);
  embedder::HandlePassingInformation handle_passing_info;
  platform_channel_pair_->PrepareToPassClientHandleToChildProcess(
      &unused, &handle_passing_info);

  base::LaunchOptions options;
#if defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#elif defined(OS_WIN)
  options.start_hidden = true;
  options.handles_to_inherit = &handle_passing_info;
#else
#error "Not supported yet."
#endif

  test_child_handle_ = SpawnChildWithOptions(test_child_main, options, false);
  platform_channel_pair_->ChildProcessLaunched();

  CHECK_NE(test_child_handle_, base::kNullProcessHandle);
}

int MultiprocessTestBase::WaitForChildShutdown() {
  CHECK_NE(test_child_handle_, base::kNullProcessHandle);

  static const int kTimeoutSeconds = 5;
  int rv = -1;
  CHECK(base::WaitForExitCodeWithTimeout(
      test_child_handle_, &rv, base::TimeDelta::FromSeconds(kTimeoutSeconds)));
  base::CloseProcessHandle(test_child_handle_);
  test_child_handle_ = base::kNullProcessHandle;
  return rv;
}

CommandLine MultiprocessTestBase::MakeCmdLine(const std::string& procname,
                                              bool debug_on_start) {
  CHECK(platform_channel_pair_.get());

  CommandLine command_line =
      base::MultiProcessTest::MakeCmdLine(procname, debug_on_start);
  embedder::HandlePassingInformation unused;
  platform_channel_pair_->PrepareToPassClientHandleToChildProcess(&command_line,
                                                                  &unused);

  return command_line;
}

// static
void MultiprocessTestBase::ChildSetup() {
  CHECK(CommandLine::InitializedForCurrentProcess());
  client_platform_handle =
      embedder::PlatformChannelPair::PassClientHandleFromParentProcess(
          *CommandLine::ForCurrentProcess());
}

// static
embedder::ScopedPlatformHandle MultiprocessTestBase::client_platform_handle;

}  // namespace test
}  // namespace mojo
