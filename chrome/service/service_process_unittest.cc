// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/crypto/rsa_private_key.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/service/service_process.h"
#include "remoting/host/host_key_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ServiceProcessTest, DISABLED_Run) {
  MessageLoopForUI main_message_loop;
  ServiceProcess process;
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_TRUE(process.Initialize(&main_message_loop, command_line));
  EXPECT_TRUE(process.Teardown());
}

#if defined(ENABLE_REMOTING)
// This test seems to break randomly so disabling it.
TEST(ServiceProcessTest, DISABLED_RunChromoting) {
  MessageLoopForUI main_message_loop;
  ServiceProcess process;
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_TRUE(process.Initialize(&main_message_loop, command_line));

  // Then config the chromoting host and start it.
  process.remoting_host_manager()->SetCredentials("email", "token");
  process.remoting_host_manager()->Enable();
  process.remoting_host_manager()->Disable();
  EXPECT_TRUE(process.Teardown());
}

class MockServiceProcess : public ServiceProcess {
 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessTest, RunChromotingUntilShutdown);
  MOCK_METHOD0(OnChromotingHostShutdown, void());
};

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

TEST(ServiceProcessTest, DISABLED_RunChromotingUntilShutdown) {
  MessageLoopForUI main_message_loop;
  MockServiceProcess process;
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_TRUE(process.Initialize(&main_message_loop, command_line));

  // Expect chromoting shutdown be called because the login token is invalid.
  EXPECT_CALL(process, OnChromotingHostShutdown())
      .WillOnce(QuitMessageLoop(&main_message_loop));

  // Then config the chromoting host and start it.
  process.remoting_host_manager()->SetCredentials("email", "token");
  process.remoting_host_manager()->Enable();
  MessageLoop::current()->Run();

  EXPECT_TRUE(process.Teardown());
}
#endif  // defined(ENABLE_REMOTING)
