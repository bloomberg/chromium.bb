// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/main_function_params.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

// TODO(port): Bring up this test this on other platforms.
#if defined(OS_POSIX)

using base::ProcessHandle;

const char kRendererTestChannelName[] = "test";

extern int RendererMain(const MainFunctionParams& parameters);

// TODO(port): The IPC Channel tests have a class with extremely similar
// functionality, we should combine them.
class RendererMainTest : public base::MultiProcessTest {
 protected:
  RendererMainTest() {}

  // Create a new MessageLoopForIO For each test.
  virtual void SetUp();
  virtual void TearDown();

  // Spawns a child process of the specified type
  base::ProcessHandle SpawnChild(const std::string& procname,
                                 IPC::Channel* channel);

  virtual CommandLine MakeCmdLine(const std::string& procname,
                                  bool debug_on_start) OVERRIDE;

  // Created around each test instantiation.
  MessageLoopForIO *message_loop_;
};

void RendererMainTest::SetUp() {
  MultiProcessTest::SetUp();

  // Construct a fresh IO Message loop for the duration of each test.
  message_loop_ = new MessageLoopForIO();
}

void RendererMainTest::TearDown() {
  delete message_loop_;
  message_loop_ = NULL;

  MultiProcessTest::TearDown();
}

ProcessHandle RendererMainTest::SpawnChild(const std::string& procname,
                                           IPC::Channel* channel) {
  base::file_handle_mapping_vector fds_to_map;
  const int ipcfd = channel->GetClientFileDescriptor();
  if (ipcfd > -1) {
    fds_to_map.push_back(std::pair<int,int>(ipcfd, 3));
  }

  return MultiProcessTest::SpawnChild(procname, fds_to_map, false);
}

CommandLine RendererMainTest::MakeCmdLine(const std::string& procname,
                                          bool debug_on_start) {
  CommandLine command_line =
      MultiProcessTest::MakeCmdLine(procname, debug_on_start);

  // Force seccomp off for this test.  It's just a problem of refactoring,
  // not a bug.
  // http://code.google.com/p/chromium/issues/detail?id=59376
  command_line.AppendSwitch(switches::kDisableSeccompSandbox);

  return command_line;
}

// Listener class that kills the message loop when it connects.
class SuicidalListener : public IPC::Channel::Listener {
 public:
  void OnChannelConnected(int32 peer_pid) {
    MessageLoop::current()->Quit();
  }

  bool OnMessageReceived(const IPC::Message& message) {
    // We shouldn't receive any messages
    NOTREACHED();
    return false;
  }
};

MULTIPROCESS_TEST_MAIN(SimpleRenderer) {
  SandboxInitWrapper dummy_sandbox_init;
  CommandLine cl(*CommandLine::ForCurrentProcess());
  cl.AppendSwitchASCII(switches::kProcessChannelID, kRendererTestChannelName);

  MainFunctionParams dummy_params(cl, dummy_sandbox_init, NULL);
  return RendererMain(dummy_params);
}

TEST_F(RendererMainTest, CreateDestroy) {
  SuicidalListener listener;
  IPC::Channel control_channel(kRendererTestChannelName,
                               IPC::Channel::MODE_SERVER,
                               &listener);
  base::ProcessHandle renderer_pid = SpawnChild("SimpleRenderer",
                                                &control_channel);

  ASSERT_TRUE(control_channel.Connect());

  MessageLoop::current()->Run();

  // The renderer should exit when we close the channel.
  control_channel.Close();

  EXPECT_TRUE(base::WaitForSingleProcess(renderer_pid,
                                         TestTimeouts::action_timeout_ms()));
}
#endif  // defined(OS_POSIX)
