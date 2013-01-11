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

// This test times the roundtrip IPC message cycle. It is enabled with a
// special preprocessor define to enable it instead of the standard IPC
// unit tests. This works around some funny termination conditions in the
// regular unit tests.
//
// This test is not automated. To test, you will want to vary the message
// count and message size in TEST to get the numbers you want.
//
// FIXME(brettw): Automate this test and have it run by default.

class IPCChannelPerfTest : public IPCTestBase {
};

// This channel listener just replies to all messages with the exact same
// message. It assumes each message has one string parameter. When the string
// "quit" is sent, it will exit.
class ChannelReflectorListener : public IPC::Listener {
 public:
  explicit ChannelReflectorListener(IPC::Channel *channel) :
    channel_(channel),
    count_messages_(0) {
    std::cout << "Reflector up" << std::endl;
  }

  ~ChannelReflectorListener() {
    std::cout << "Client Messages: " << count_messages_ << std::endl;
    std::cout << "Client Latency: " << latency_messages_.InMilliseconds()
              << std::endl;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    count_messages_++;
    PickleIterator iter(message);
    int64 time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    std::string payload;
    EXPECT_TRUE(iter.ReadString(&payload));
    // TODO(vtl): Should we use |HighResNow()| instead of |Now()|?
    latency_messages_ += base::TimeTicks::Now() -
                         base::TimeTicks::FromInternalValue(time_internal);

    // cout << "reflector msg received: " << msgid << endl;
    if (payload == "quit")
      MessageLoop::current()->Quit();

    IPC::Message* msg = new IPC::Message(0,
                                         2,
                                         IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(msgid);
    msg->WriteString(payload);
    channel_->Send(msg);
    return true;
  }

 private:
  IPC::Channel *channel_;
  int count_messages_;
  base::TimeDelta latency_messages_;
};

class ChannelPerfListener : public IPC::Listener {
 public:
  ChannelPerfListener(IPC::Channel* channel, int msg_count, int msg_size) :
       count_down_(msg_count),
       channel_(channel),
       count_messages_(0) {
    payload_.resize(msg_size);
    for (int i = 0; i < static_cast<int>(payload_.size()); i++)
      payload_[i] = 'a';
    std::cout << "perflistener up" << std::endl;
  }

  ~ChannelPerfListener() {
    std::cout << "Server Messages: " << count_messages_ << std::endl;
    std::cout << "Server Latency: " << latency_messages_.InMilliseconds()
              << std::endl;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    count_messages_++;
    // Decode the string so this gets counted in the total time.
    PickleIterator iter(message);
    int64 time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    std::string cur;
    EXPECT_TRUE(iter.ReadString(&cur));
    latency_messages_ += base::TimeTicks::Now() -
                         base::TimeTicks::FromInternalValue(time_internal);

    count_down_--;
    if (count_down_ == 0) {
      IPC::Message* msg = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
      msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
      msg->WriteInt(count_down_);
      msg->WriteString("quit");
      channel_->Send(msg);

      MessageLoop::current()->QuitWhenIdle();
      return true;
    }

    IPC::Message* msg = new IPC::Message(0,
                                         2,
                                         IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(count_down_);
    msg->WriteString(payload_);
    channel_->Send(msg);
    return true;
  }

 private:
  int count_down_;
  std::string payload_;
  IPC::Channel *channel_;
  int count_messages_;
  base::TimeDelta latency_messages_;
};

TEST_F(IPCChannelPerfTest, Performance) {
  // setup IPC channel
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_SERVER, NULL);
  ChannelPerfListener perf_listener(&chan, 10000, 100000);
  chan.set_listener(&perf_listener);
  ASSERT_TRUE(chan.Connect());

  base::ProcessHandle process_handle = SpawnChild(TEST_REFLECTOR, &chan);
  ASSERT_TRUE(process_handle);

  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));

  PerfTimeLogger logger("IPC_Perf");

  // this initial message will kick-start the ping-pong of messages
  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
  message->WriteInt(-1);
  message->WriteString("Hello");
  chan.Send(message);

  // run message loop
  MessageLoop::current()->Run();

  // Clean up child process.
  EXPECT_TRUE(base::WaitForSingleProcess(
      process_handle, base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(process_handle);
}

// This message loop bounces all messages back to the sender
MULTIPROCESS_IPC_TEST_MAIN(RunReflector) {
  MessageLoopForIO main_message_loop;
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_CLIENT, NULL);
  ChannelReflectorListener channel_reflector_listener(&chan);
  chan.set_listener(&channel_reflector_listener);
  CHECK(chan.Connect());

  MessageLoop::current()->Run();
  return 0;
}
