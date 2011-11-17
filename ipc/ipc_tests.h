// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_TESTS_H_
#define IPC_IPC_TESTS_H_
#pragma once

#include "base/test/multiprocess_test.h"
#include "base/process.h"

// This unit test uses 3 types of child processes, a regular pipe client,
// a client reflector and a IPC server used for fuzzing tests.
enum ChildType {
  TEST_CLIENT,
  TEST_DESCRIPTOR_CLIENT,
  TEST_DESCRIPTOR_CLIENT_SANDBOXED,
  TEST_REFLECTOR,
  FUZZER_SERVER,
  SYNC_SOCKET_SERVER
};

// The different channel names for the child processes.
extern const char kTestClientChannel[];
extern const char kReflectorChannel[];
extern const char kFuzzerChannel[];
extern const char kSyncSocketChannel[];

class MessageLoopForIO;
namespace IPC {
class Channel;
}  // namespace IPC

//Base class to facilitate Spawning IPC Client processes.
class IPCChannelTest : public base::MultiProcessTest {
 protected:

  // Create a new MessageLoopForIO For each test.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Spawns a child process of the specified type
  base::ProcessHandle SpawnChild(ChildType child_type, IPC::Channel* channel);

  // Created around each test instantiation.
  MessageLoopForIO* message_loop_;
};

#endif  // IPC_IPC_TESTS_H_
