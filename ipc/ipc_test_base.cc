// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <string>
#include <utility>

#include "ipc/ipc_test_base.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debug_on_start_win.h"
#include "base/perftimer.h"
#include "base/pickle.h"
#include "base/test/perf_test_suite.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_multiprocess_test.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_switches.h"
#include "testing/multiprocess_func_list.h"

// Define to enable IPC performance testing instead of the regular unit tests
// #define PERFORMANCE_TEST

const char kTestClientChannel[] = "T1";
const char kReflectorChannel[] = "T2";
const char kFuzzerChannel[] = "F3";
const char kSyncSocketChannel[] = "S4";

void IPCTestBase::SetUp() {
  MultiProcessTest::SetUp();

  // Construct a fresh IO Message loop for the duration of each test.
  message_loop_ = new MessageLoopForIO();
}

void IPCTestBase::TearDown() {
  delete message_loop_;
  message_loop_ = NULL;

  MultiProcessTest::TearDown();
}

#if defined(OS_WIN)
base::ProcessHandle IPCTestBase::SpawnChild(IPCTestBase::ChildType child_type,
                                            IPC::Channel *channel) {
  // kDebugChildren support.
  bool debug_on_start =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDebugChildren);

  switch (child_type) {
  case TEST_CLIENT:
    return MultiProcessTest::SpawnChild("RunTestClient", debug_on_start);
  case TEST_REFLECTOR:
    return MultiProcessTest::SpawnChild("RunReflector", debug_on_start);
  case FUZZER_SERVER:
    return MultiProcessTest::SpawnChild("RunFuzzServer", debug_on_start);
  case SYNC_SOCKET_SERVER:
    return MultiProcessTest::SpawnChild("RunSyncSocketServer", debug_on_start);
  default:
    return NULL;
  }
}
#elif defined(OS_POSIX)
base::ProcessHandle IPCTestBase::SpawnChild(IPCTestBase::ChildType child_type,
                                            IPC::Channel *channel) {
  // kDebugChildren support.
  bool debug_on_start =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDebugChildren);

  base::FileHandleMappingVector fds_to_map;
  const int ipcfd = channel->GetClientFileDescriptor();
  if (ipcfd > -1) {
    fds_to_map.push_back(std::pair<int, int>(ipcfd, kPrimaryIPCChannel + 3));
  }

  base::ProcessHandle ret = base::kNullProcessHandle;
  switch (child_type) {
  case TEST_CLIENT:
    ret = MultiProcessTest::SpawnChild("RunTestClient",
                                       fds_to_map,
                                       debug_on_start);
    break;
  case TEST_DESCRIPTOR_CLIENT:
    ret = MultiProcessTest::SpawnChild("RunTestDescriptorClient",
                                       fds_to_map,
                                       debug_on_start);
    break;
  case TEST_DESCRIPTOR_CLIENT_SANDBOXED:
    ret = MultiProcessTest::SpawnChild("RunTestDescriptorClientSandboxed",
                                       fds_to_map,
                                       debug_on_start);
    break;
  case TEST_REFLECTOR:
    ret = MultiProcessTest::SpawnChild("RunReflector",
                                       fds_to_map,
                                       debug_on_start);
    break;
  case FUZZER_SERVER:
    ret = MultiProcessTest::SpawnChild("RunFuzzServer",
                                       fds_to_map,
                                       debug_on_start);
    break;
  case SYNC_SOCKET_SERVER:
    ret = MultiProcessTest::SpawnChild("RunSyncSocketServer",
                                       fds_to_map,
                                       debug_on_start);
    break;
  default:
    return base::kNullProcessHandle;
    break;
  }
  return ret;
}
#endif  // defined(OS_POSIX)
