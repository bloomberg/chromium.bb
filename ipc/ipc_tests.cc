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

#include "ipc/ipc_tests.h"

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

const size_t kLongMessageStringNumBytes = 50000;

void IPCChannelTest::SetUp() {
  MultiProcessTest::SetUp();

  // Construct a fresh IO Message loop for the duration of each test.
  message_loop_ = new MessageLoopForIO();
}

void IPCChannelTest::TearDown() {
  delete message_loop_;
  message_loop_ = NULL;

  MultiProcessTest::TearDown();
}

#if defined(OS_WIN)
base::ProcessHandle IPCChannelTest::SpawnChild(ChildType child_type,
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
base::ProcessHandle IPCChannelTest::SpawnChild(ChildType child_type,
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

#ifndef PERFORMANCE_TEST
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

#endif  // !PERFORMANCE_TEST

#ifdef PERFORMANCE_TEST

//-----------------------------------------------------------------------------
// Manually performance test
//
//    This test times the roundtrip IPC message cycle. It is enabled with a
//    special preprocessor define to enable it instead of the standard IPC
//    unit tests. This works around some funny termination conditions in the
//    regular unit tests.
//
//    This test is not automated. To test, you will want to vary the message
//    count and message size in TEST to get the numbers you want.
//
//    FIXME(brettw): Automate this test and have it run by default.

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

TEST_F(IPCChannelTest, Performance) {
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

#endif  // PERFORMANCE_TEST

int main(int argc, char** argv) {
#ifdef PERFORMANCE_TEST
  int retval = base::PerfTestSuite(argc, argv).Run();
#else
  int retval = base::TestSuite(argc, argv).Run();
#endif
  return retval;
}
