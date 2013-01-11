// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <sstream>

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/threading/platform_thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_multiprocess_test.h"
#include "ipc/ipc_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

// IPC messages for testing ----------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

// Generic message class that is an int followed by a wstring.
IPC_MESSAGE_CONTROL2(MsgClassIS, int, std::wstring)

// Generic message class that is a wstring followed by an int.
IPC_MESSAGE_CONTROL2(MsgClassSI, std::wstring, int)

// Message to create a mutex in the IPC server, using the received name.
IPC_MESSAGE_CONTROL2(MsgDoMutex, std::wstring, int)

// Used to generate an ID for a message that should not exist.
IPC_MESSAGE_CONTROL0(MsgUnhandled)

// -----------------------------------------------------------------------------

namespace {

TEST(IPCMessageIntegrity, ReadBeyondBufferStr) {
  //This was BUG 984408.
  uint32 v1 = kuint32max - 1;
  int v2 = 666;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteInt(v2));

  PickleIterator iter(m);
  std::string vs;
  EXPECT_FALSE(m.ReadString(&iter, &vs));
}

TEST(IPCMessageIntegrity, ReadBeyondBufferWStr) {
  //This was BUG 984408.
  uint32 v1 = kuint32max - 1;
  int v2 = 777;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteInt(v2));

  PickleIterator iter(m);
  std::wstring vs;
  EXPECT_FALSE(m.ReadWString(&iter, &vs));
}

TEST(IPCMessageIntegrity, ReadBytesBadIterator) {
  // This was BUG 1035467.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(1));
  EXPECT_TRUE(m.WriteInt(2));

  PickleIterator iter(m);
  const char* data = NULL;
  EXPECT_TRUE(m.ReadBytes(&iter, &data, sizeof(int)));
}

TEST(IPCMessageIntegrity, ReadVectorNegativeSize) {
  // A slight variation of BUG 984408. Note that the pickling of vector<char>
  // has a specialized template which is not vulnerable to this bug. So here
  // try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(-1));   // This is the count of elements.
  EXPECT_TRUE(m.WriteInt(1));
  EXPECT_TRUE(m.WriteInt(2));
  EXPECT_TRUE(m.WriteInt(3));

  std::vector<double> vec;
  PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

TEST(IPCMessageIntegrity, ReadVectorTooLarge1) {
  // This was BUG 1006367. This is the large but positive length case. Again
  // we try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(0x21000003));   // This is the count of elements.
  EXPECT_TRUE(m.WriteInt64(1));
  EXPECT_TRUE(m.WriteInt64(2));

  std::vector<int64> vec;
  PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

TEST(IPCMessageIntegrity, ReadVectorTooLarge2) {
  // This was BUG 1006367. This is the large but positive with an additional
  // integer overflow when computing the actual byte size. Again we try to hit
  // the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(0x71000000));   // This is the count of elements.
  EXPECT_TRUE(m.WriteInt64(1));
  EXPECT_TRUE(m.WriteInt64(2));

  std::vector<int64> vec;
  PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

class SimpleListener : public IPC::Listener {
 public:
  SimpleListener() : other_(NULL) {
  }
  void Init(IPC::Sender* s) {
    other_ = s;
  }
 protected:
  IPC::Sender* other_;
};

enum {
  FUZZER_ROUTING_ID = 5
};

// The fuzzer server class. It runs in a child process and expects
// only two IPC calls; after that it exits the message loop which
// terminates the child process.
class FuzzerServerListener : public SimpleListener {
 public:
  FuzzerServerListener() : message_count_(2), pending_messages_(0) {
  }
  virtual bool OnMessageReceived(const IPC::Message& msg) {
    if (msg.routing_id() == MSG_ROUTING_CONTROL) {
      ++pending_messages_;
      IPC_BEGIN_MESSAGE_MAP(FuzzerServerListener, msg)
        IPC_MESSAGE_HANDLER(MsgClassIS, OnMsgClassISMessage)
        IPC_MESSAGE_HANDLER(MsgClassSI, OnMsgClassSIMessage)
      IPC_END_MESSAGE_MAP()
      if (pending_messages_) {
        // Probably a problem de-serializing the message.
        ReplyMsgNotHandled(msg.type());
      }
    }
    return true;
  }

 private:
  void OnMsgClassISMessage(int value, const std::wstring& text) {
    UseData(MsgClassIS::ID, value, text);
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgClassIS::ID, value);
    Cleanup();
  }

  void OnMsgClassSIMessage(const std::wstring& text, int value) {
    UseData(MsgClassSI::ID, value, text);
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgClassSI::ID, value);
    Cleanup();
  }

  bool RoundtripAckReply(int routing, uint32 type_id, int reply) {
    IPC::Message* message = new IPC::Message(routing, type_id,
                                             IPC::Message::PRIORITY_NORMAL);
    message->WriteInt(reply + 1);
    message->WriteInt(reply);
    return other_->Send(message);
  }

  void Cleanup() {
    --message_count_;
    --pending_messages_;
    if (0 == message_count_)
      MessageLoop::current()->Quit();
  }

  void ReplyMsgNotHandled(uint32 type_id) {
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgUnhandled::ID, type_id);
    Cleanup();
  }

  void UseData(int caller, int value, const std::wstring& text) {
    std::wostringstream wos;
    wos << L"IPC fuzzer:" << caller << " [" << value << L" " << text << L"]\n";
    std::wstring output = wos.str();
    LOG(WARNING) << output.c_str();
  };

  int message_count_;
  int pending_messages_;
};

class FuzzerClientListener : public SimpleListener {
 public:
  FuzzerClientListener() : last_msg_(NULL) {
  }

  virtual bool OnMessageReceived(const IPC::Message& msg) {
    last_msg_ = new IPC::Message(msg);
    MessageLoop::current()->Quit();
    return true;
  }

  bool ExpectMessage(int value, uint32 type_id) {
    if (!MsgHandlerInternal(type_id))
      return false;
    int msg_value1 = 0;
    int msg_value2 = 0;
    PickleIterator iter(*last_msg_);
    if (!last_msg_->ReadInt(&iter, &msg_value1))
      return false;
    if (!last_msg_->ReadInt(&iter, &msg_value2))
      return false;
    if ((msg_value2 + 1) != msg_value1)
      return false;
    if (msg_value2 != value)
      return false;

    delete last_msg_;
    last_msg_ = NULL;
    return true;
  }

  bool ExpectMsgNotHandled(uint32 type_id) {
    return ExpectMessage(type_id, MsgUnhandled::ID);
  }

 private:
  bool MsgHandlerInternal(uint32 type_id) {
    MessageLoop::current()->Run();
    if (NULL == last_msg_)
      return false;
    if (FUZZER_ROUTING_ID != last_msg_->routing_id())
      return false;
    return (type_id == last_msg_->type());
  };

  IPC::Message* last_msg_;
};

// Runs the fuzzing server child mode. Returns when the preset number
// of messages have been received.
MULTIPROCESS_IPC_TEST_MAIN(RunFuzzServer) {
  MessageLoopForIO main_message_loop;
  FuzzerServerListener listener;
  IPC::Channel chan(kFuzzerChannel, IPC::Channel::MODE_CLIENT, &listener);
  CHECK(chan.Connect());
  listener.Init(&chan);
  MessageLoop::current()->Run();
  return 0;
}

class IPCFuzzingTest : public IPCTestBase {
};

// This test makes sure that the FuzzerClientListener and FuzzerServerListener
// are working properly by generating two well formed IPC calls.
TEST_F(IPCFuzzingTest, SanityTest) {
  FuzzerClientListener listener;
  IPC::Channel chan(kFuzzerChannel, IPC::Channel::MODE_SERVER,
                    &listener);
  base::ProcessHandle server_process = SpawnChild(FUZZER_SERVER, &chan);
  ASSERT_TRUE(server_process);
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  ASSERT_TRUE(chan.Connect());
  listener.Init(&chan);

  IPC::Message* msg = NULL;
  int value = 43;
  msg = new MsgClassIS(value, L"expect 43");
  chan.Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(value, MsgClassIS::ID));

  msg = new MsgClassSI(L"expect 44", ++value);
  chan.Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(value, MsgClassSI::ID));

  EXPECT_TRUE(base::WaitForSingleProcess(
      server_process, base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(server_process);
}

// This test uses a payload that is smaller than expected.
// This generates an error while unpacking the IPC buffer which in
// In debug this triggers an assertion and in release it is ignored(!!). Right
// after we generate another valid IPC to make sure framing is working
// properly.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(IPCFuzzingTest, MsgBadPayloadShort) {
  FuzzerClientListener listener;
  IPC::Channel chan(kFuzzerChannel, IPC::Channel::MODE_SERVER,
                    &listener);
  base::ProcessHandle server_process = SpawnChild(FUZZER_SERVER, &chan);
  ASSERT_TRUE(server_process);
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  ASSERT_TRUE(chan.Connect());
  listener.Init(&chan);

  IPC::Message* msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassIS::ID,
                                       IPC::Message::PRIORITY_NORMAL);
  msg->WriteInt(666);
  chan.Send(msg);
  EXPECT_TRUE(listener.ExpectMsgNotHandled(MsgClassIS::ID));

  msg = new MsgClassSI(L"expect one", 1);
  chan.Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(1, MsgClassSI::ID));

  EXPECT_TRUE(base::WaitForSingleProcess(
      server_process, base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(server_process);
}
#endif

// This test uses a payload that has too many arguments, but so the payload
// size is big enough so the unpacking routine does not generate an error as
// in the case of MsgBadPayloadShort test.
// This test does not pinpoint a flaw (per se) as by design we don't carry
// type information on the IPC message.
TEST_F(IPCFuzzingTest, MsgBadPayloadArgs) {
  FuzzerClientListener listener;
  IPC::Channel chan(kFuzzerChannel, IPC::Channel::MODE_SERVER,
                    &listener);
  base::ProcessHandle server_process = SpawnChild(FUZZER_SERVER, &chan);
  ASSERT_TRUE(server_process);
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  ASSERT_TRUE(chan.Connect());
  listener.Init(&chan);

  IPC::Message* msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassSI::ID,
                                       IPC::Message::PRIORITY_NORMAL);
  msg->WriteWString(L"d");
  msg->WriteInt(0);
  msg->WriteInt(0x65);  // Extra argument.

  chan.Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(0, MsgClassSI::ID));

  // Now send a well formed message to make sure the receiver wasn't
  // thrown out of sync by the extra argument.
  msg = new MsgClassIS(3, L"expect three");
  chan.Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(3, MsgClassIS::ID));

  EXPECT_TRUE(base::WaitForSingleProcess(
      server_process, base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(server_process);
}

// This class is for testing the IPC_BEGIN_MESSAGE_MAP_EX macros.
class ServerMacroExTest {
 public:
  ServerMacroExTest() : unhandled_msgs_(0) {
  }

  virtual ~ServerMacroExTest() {
  }

  virtual bool OnMessageReceived(const IPC::Message& msg) {
    bool msg_is_ok = false;
    IPC_BEGIN_MESSAGE_MAP_EX(ServerMacroExTest, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(MsgClassIS, OnMsgClassISMessage)
      IPC_MESSAGE_HANDLER(MsgClassSI, OnMsgClassSIMessage)
      IPC_MESSAGE_UNHANDLED(++unhandled_msgs_)
    IPC_END_MESSAGE_MAP_EX()
    return msg_is_ok;
  }

  int unhandled_msgs() const {
    return unhandled_msgs_;
  }

 private:
  void OnMsgClassISMessage(int value, const std::wstring& text) {
  }
  void OnMsgClassSIMessage(const std::wstring& text, int value) {
  }

  int unhandled_msgs_;

  DISALLOW_COPY_AND_ASSIGN(ServerMacroExTest);
};

TEST_F(IPCFuzzingTest, MsgMapExMacro) {
  IPC::Message* msg = NULL;
  ServerMacroExTest server;

  // Test the regular messages.
  msg = new MsgClassIS(3, L"text3");
  EXPECT_TRUE(server.OnMessageReceived(*msg));
  delete msg;
  msg = new MsgClassSI(L"text2", 2);
  EXPECT_TRUE(server.OnMessageReceived(*msg));
  delete msg;

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
  // Test a bad message.
  msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassSI::ID,
                         IPC::Message::PRIORITY_NORMAL);
  msg->WriteInt(2);
  EXPECT_FALSE(server.OnMessageReceived(*msg));
  delete msg;

  msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassIS::ID,
                         IPC::Message::PRIORITY_NORMAL);
  msg->WriteInt(0x64);
  msg->WriteInt(0x32);
  EXPECT_FALSE(server.OnMessageReceived(*msg));
  delete msg;

  EXPECT_EQ(0, server.unhandled_msgs());
#endif
}

}  // namespace
