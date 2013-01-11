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
#include "ipc/ipc_test_base.h"
#include "testing/multiprocess_func_list.h"

namespace {

const size_t kLongMessageStringNumBytes = 50000;

class IPCChannelTest : public IPCTestBase {
};

TEST_F(IPCChannelTest, BasicMessageTest) {
  int v1 = 10;
  std::string v2("foobar");
  std::wstring v3(L"hello world");

  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteString(v2));
  EXPECT_TRUE(m.WriteWString(v3));

  PickleIterator iter(m);

  int vi;
  std::string vs;
  std::wstring vw;

  EXPECT_TRUE(m.ReadInt(&iter, &vi));
  EXPECT_EQ(v1, vi);

  EXPECT_TRUE(m.ReadString(&iter, &vs));
  EXPECT_EQ(v2, vs);

  EXPECT_TRUE(m.ReadWString(&iter, &vw));
  EXPECT_EQ(v3, vw);

  // should fail
  EXPECT_FALSE(m.ReadInt(&iter, &vi));
  EXPECT_FALSE(m.ReadString(&iter, &vs));
  EXPECT_FALSE(m.ReadWString(&iter, &vw));
}

static void Send(IPC::Sender* sender, const char* text) {
  static int message_index = 0;

  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(message_index++);
  message->WriteString(std::string(text));

  // Make sure we can handle large messages.
  char junk[kLongMessageStringNumBytes];
  memset(junk, 'a', sizeof(junk)-1);
  junk[sizeof(junk)-1] = 0;
  message->WriteString(std::string(junk));

  // DEBUG: printf("[%u] sending message [%s]\n", GetCurrentProcessId(), text);
  sender->Send(message);
}

class MyChannelListener : public IPC::Listener {
 public:
  virtual bool OnMessageReceived(const IPC::Message& message) {
    PickleIterator iter(message);

    int ignored;
    EXPECT_TRUE(iter.ReadInt(&ignored));
    std::string data;
    EXPECT_TRUE(iter.ReadString(&data));
    std::string big_string;
    EXPECT_TRUE(iter.ReadString(&big_string));
    EXPECT_EQ(kLongMessageStringNumBytes - 1, big_string.length());


    if (--messages_left_ == 0) {
      MessageLoop::current()->Quit();
    } else {
      Send(sender_, "Foo");
    }
    return true;
  }

  virtual void OnChannelError() {
    // There is a race when closing the channel so the last message may be lost.
    EXPECT_LE(messages_left_, 1);
    MessageLoop::current()->Quit();
  }

  void Init(IPC::Sender* s) {
    sender_ = s;
    messages_left_ = 50;
  }

 private:
  IPC::Sender* sender_;
  int messages_left_;
};

TEST_F(IPCChannelTest, ChannelTest) {
  MyChannelListener channel_listener;
  // Setup IPC channel.
  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_SERVER,
                    &channel_listener);
  ASSERT_TRUE(chan.Connect());

  channel_listener.Init(&chan);

  base::ProcessHandle process_handle = SpawnChild(TEST_CLIENT, &chan);
  ASSERT_TRUE(process_handle);

  Send(&chan, "hello from parent");

  // Run message loop.
  MessageLoop::current()->Run();

  // Close Channel so client gets its OnChannelError() callback fired.
  chan.Close();

  // Cleanup child process.
  EXPECT_TRUE(base::WaitForSingleProcess(
      process_handle, base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(process_handle);
}

#if defined(OS_WIN)
TEST_F(IPCChannelTest, ChannelTestExistingPipe) {
  MyChannelListener channel_listener;
  // Setup IPC channel with existing pipe. Specify name in Chrome format.
  std::string name("\\\\.\\pipe\\chrome.");
  name.append(kTestClientChannel);
  const DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                          FILE_FLAG_FIRST_PIPE_INSTANCE;
  HANDLE pipe = CreateNamedPipeA(name.c_str(),
                                 open_mode,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                                 1,
                                 4096,
                                 4096,
                                 5000,
                                 NULL);
  IPC::Channel chan(IPC::ChannelHandle(pipe), IPC::Channel::MODE_SERVER,
                    &channel_listener);
  // Channel will duplicate the handle.
  CloseHandle(pipe);
  ASSERT_TRUE(chan.Connect());

  channel_listener.Init(&chan);

  base::ProcessHandle process_handle = SpawnChild(TEST_CLIENT, &chan);
  ASSERT_TRUE(process_handle);

  Send(&chan, "hello from parent");

  // Run message loop.
  MessageLoop::current()->Run();

  // Close Channel so client gets its OnChannelError() callback fired.
  chan.Close();

  // Cleanup child process.
  EXPECT_TRUE(base::WaitForSingleProcess(
      process_handle, base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(process_handle);
}
#endif  // defined (OS_WIN)

TEST_F(IPCChannelTest, ChannelProxyTest) {
  MyChannelListener channel_listener;

  // The thread needs to out-live the ChannelProxy.
  base::Thread thread("ChannelProxyTestServer");
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  thread.StartWithOptions(options);
  {
    // setup IPC channel proxy
    IPC::ChannelProxy chan(kTestClientChannel, IPC::Channel::MODE_SERVER,
                           &channel_listener, thread.message_loop_proxy());

    channel_listener.Init(&chan);

#if defined(OS_WIN)
    base::ProcessHandle process_handle = SpawnChild(TEST_CLIENT, NULL);
#elif defined(OS_POSIX)
    bool debug_on_start = CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kDebugChildren);
    base::FileHandleMappingVector fds_to_map;
    const int ipcfd = chan.GetClientFileDescriptor();
    if (ipcfd > -1) {
      fds_to_map.push_back(std::pair<int, int>(ipcfd, kPrimaryIPCChannel + 3));
    }

    base::ProcessHandle process_handle = MultiProcessTest::SpawnChild(
        "RunTestClient",
        fds_to_map,
        debug_on_start);
#endif  // defined(OS_POSIX)

    ASSERT_TRUE(process_handle);

    Send(&chan, "hello from parent");

    // run message loop
    MessageLoop::current()->Run();

    // cleanup child process
    EXPECT_TRUE(base::WaitForSingleProcess(
        process_handle, base::TimeDelta::FromSeconds(5)));
    base::CloseProcessHandle(process_handle);
  }
  thread.Stop();
}

class ChannelListenerWithOnConnectedSend : public IPC::Listener {
 public:
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
    SendNextMessage();
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    PickleIterator iter(message);

    int ignored;
    EXPECT_TRUE(iter.ReadInt(&ignored));
    std::string data;
    EXPECT_TRUE(iter.ReadString(&data));
    std::string big_string;
    EXPECT_TRUE(iter.ReadString(&big_string));
    EXPECT_EQ(kLongMessageStringNumBytes - 1, big_string.length());
    SendNextMessage();
    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    // There is a race when closing the channel so the last message may be lost.
    EXPECT_LE(messages_left_, 1);
    MessageLoop::current()->Quit();
  }

  void Init(IPC::Sender* s) {
    sender_ = s;
    messages_left_ = 50;
  }

 private:
  void SendNextMessage() {
    if (--messages_left_ == 0) {
      MessageLoop::current()->Quit();
    } else {
      Send(sender_, "Foo");
    }
  }

  IPC::Sender* sender_;
  int messages_left_;
};

#if defined(OS_WIN)
// Acting flakey in Windows. http://crbug.com/129595
#define MAYBE_SendMessageInChannelConnected DISABLED_SendMessageInChannelConnected
#else
#define MAYBE_SendMessageInChannelConnected SendMessageInChannelConnected
#endif
TEST_F(IPCChannelTest, MAYBE_SendMessageInChannelConnected) {
  // This tests the case of a listener sending back an event in it's
  // OnChannelConnected handler.

  ChannelListenerWithOnConnectedSend channel_listener;
  // Setup IPC channel.
  IPC::Channel channel(kTestClientChannel, IPC::Channel::MODE_SERVER,
                       &channel_listener);
  channel_listener.Init(&channel);
  ASSERT_TRUE(channel.Connect());

  base::ProcessHandle process_handle = SpawnChild(TEST_CLIENT, &channel);
  ASSERT_TRUE(process_handle);

  Send(&channel, "hello from parent");

  // Run message loop.
  MessageLoop::current()->Run();

  // Close Channel so client gets its OnChannelError() callback fired.
  channel.Close();

  // Cleanup child process.
  EXPECT_TRUE(base::WaitForSingleProcess(
      process_handle, base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(process_handle);
}

MULTIPROCESS_IPC_TEST_MAIN(RunTestClient) {
  MessageLoopForIO main_message_loop;
  MyChannelListener channel_listener;

  // setup IPC channel
  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_CLIENT,
                    &channel_listener);
  CHECK(chan.Connect());
  channel_listener.Init(&chan);
  Send(&chan, "hello from child");
  // run message loop
  MessageLoop::current()->Run();
  return 0;
}

}  // namespace
