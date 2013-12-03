// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/multiprocess_test_base.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
// TODO(vtl): Remove build_config.h include when fully implemented on Windows.
#include "build/build_config.h"

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

// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  platform_server_channel =
      system::PlatformServerChannel::Create("TestChannel");
#endif
}

void MultiprocessTestBase::TearDown() {
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);

  platform_server_channel.reset();

  MultiProcessTest::TearDown();
}

void MultiprocessTestBase::StartChild(const std::string& test_child_name) {
  CHECK(platform_server_channel.get());
  CHECK(!test_child_name.empty());
  CHECK_EQ(test_child_handle_, base::kNullProcessHandle);

  std::string test_child_main = test_child_name + "TestChildMain";

#if defined(OS_POSIX)
  CommandLine unused(CommandLine::NO_PROGRAM);
  base::FileHandleMappingVector fds_to_map;
  platform_server_channel->GetDataNeededToPassClientChannelToChildProcess(
      &unused, &fds_to_map);
  test_child_handle_ = SpawnChild(test_child_main, fds_to_map, false);
#elif defined(OS_WIN)
  test_child_handle_  = SpawnChild(test_child_main, false);
#else
#error "Not supported yet."
#endif
// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  platform_server_channel->ChildProcessLaunched();
#endif

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
  CHECK(platform_server_channel.get());

  CommandLine command_line =
      base::MultiProcessTest::MakeCmdLine(procname, debug_on_start);
// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  base::FileHandleMappingVector unused;
  platform_server_channel->GetDataNeededToPassClientChannelToChildProcess(
      &command_line, &unused);
#endif
  return command_line;
}

// static
void MultiprocessTestBase::ChildSetup() {
  CHECK(CommandLine::InitializedForCurrentProcess());
// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  platform_client_channel =
      system::PlatformClientChannel::CreateFromParentProcess(
          *CommandLine::ForCurrentProcess());
  CHECK(platform_client_channel.get());
#endif
}

// static
scoped_ptr<system::PlatformClientChannel>
    MultiprocessTestBase::platform_client_channel;

}  // namespace test
}  // namespace mojo
