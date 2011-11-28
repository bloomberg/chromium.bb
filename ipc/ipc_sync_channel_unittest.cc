// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for SyncChannel.

#include "ipc/ipc_sync_channel.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_sync_message_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;

namespace IPC {

namespace {

// Base class for a "process" with listener and IPC threads.
class Worker : public Channel::Listener, public Message::Sender {
 public:
  // Will create a channel without a name.
  Worker(Channel::Mode mode, const std::string& thread_name)
      : done_(new WaitableEvent(false, false)),
        channel_created_(new WaitableEvent(false, false)),
        mode_(mode),
        ipc_thread_((thread_name + "_ipc").c_str()),
        listener_thread_((thread_name + "_listener").c_str()),
        overrided_thread_(NULL),
        shutdown_event_(true, false) {
    // The data race on vfptr is real but is very hard
    // to suppress using standard Valgrind mechanism (suppressions).
    // We have to use ANNOTATE_BENIGN_RACE to hide the reports and
    // make ThreadSanitizer bots green.
    ANNOTATE_BENIGN_RACE(this, "Race on vfptr, http://crbug.com/25841");
  }

  // Will create a named channel and use this name for the threads' name.
  Worker(const std::string& channel_name, Channel::Mode mode)
      : done_(new WaitableEvent(false, false)),
        channel_created_(new WaitableEvent(false, false)),
        channel_name_(channel_name),
        mode_(mode),
        ipc_thread_((channel_name + "_ipc").c_str()),
        listener_thread_((channel_name + "_listener").c_str()),
        overrided_thread_(NULL),
        shutdown_event_(true, false) {
    // The data race on vfptr is real but is very hard
    // to suppress using standard Valgrind mechanism (suppressions).
    // We have to use ANNOTATE_BENIGN_RACE to hide the reports and
    // make ThreadSanitizer bots green.
    ANNOTATE_BENIGN_RACE(this, "Race on vfptr, http://crbug.com/25841");
  }

  // The IPC thread needs to outlive SyncChannel, so force the correct order of
  // destruction.
  virtual ~Worker() {
    WaitableEvent listener_done(false, false), ipc_done(false, false);
    ListenerThread()->message_loop()->PostTask(
        FROM_HERE, base::Bind(&Worker::OnListenerThreadShutdown1, this,
                              &listener_done, &ipc_done));
    listener_done.Wait();
    ipc_done.Wait();
    ipc_thread_.Stop();
    listener_thread_.Stop();
  }
  void AddRef() { }
  void Release() { }
  static bool ImplementsThreadSafeReferenceCounting() { return true; }
  bool Send(Message* msg) { return channel_->Send(msg); }
  bool SendWithTimeout(Message* msg, int timeout_ms) {
    return channel_->SendWithTimeout(msg, timeout_ms);
  }
  void WaitForChannelCreation() { channel_created_->Wait(); }
  void CloseChannel() {
    DCHECK(MessageLoop::current() == ListenerThread()->message_loop());
    channel_->Close();
  }
  void Start() {
    StartThread(&listener_thread_, MessageLoop::TYPE_DEFAULT);
    ListenerThread()->message_loop()->PostTask(
        FROM_HERE, base::Bind(&Worker::OnStart, this));
  }
  void OverrideThread(base::Thread* overrided_thread) {
    DCHECK(overrided_thread_ == NULL);
    overrided_thread_ = overrided_thread;
  }
  bool SendAnswerToLife(bool pump, int timeout, bool succeed) {
    int answer = 0;
    SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump)
      msg->EnableMessagePumping();
    bool result = SendWithTimeout(msg, timeout);
    DCHECK_EQ(result, succeed);
    DCHECK_EQ(answer, (succeed ? 42 : 0));
    return result;
  }
  bool SendDouble(bool pump, bool succeed) {
    int answer = 0;
    SyncMessage* msg = new SyncChannelTestMsg_Double(5, &answer);
    if (pump)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK_EQ(result, succeed);
    DCHECK_EQ(answer, (succeed ? 10 : 0));
    return result;
  }
  const std::string& channel_name() { return channel_name_; }
  Channel::Mode mode() { return mode_; }
  WaitableEvent* done_event() { return done_.get(); }
  WaitableEvent* shutdown_event() { return &shutdown_event_; }
  void ResetChannel() { channel_.reset(); }
  // Derived classes need to call this when they've completed their part of
  // the test.
  void Done() { done_->Signal(); }

 protected:
  SyncChannel* channel() { return channel_.get(); }
  // Functions for dervied classes to implement if they wish.
  virtual void Run() { }
  virtual void OnAnswer(int* answer) { NOTREACHED(); }
  virtual void OnAnswerDelay(Message* reply_msg) {
    // The message handler map below can only take one entry for
    // SyncChannelTestMsg_AnswerToLife, so since some classes want
    // the normal version while other want the delayed reply, we
    // call the normal version if the derived class didn't override
    // this function.
    int answer;
    OnAnswer(&answer);
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, answer);
    Send(reply_msg);
  }
  virtual void OnDouble(int in, int* out) { NOTREACHED(); }
  virtual void OnDoubleDelay(int in, Message* reply_msg) {
    int result;
    OnDouble(in, &result);
    SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, result);
    Send(reply_msg);
  }

  virtual void OnNestedTestMsg(Message* reply_msg) {
    NOTREACHED();
  }

  virtual SyncChannel* CreateChannel() {
    return new SyncChannel(
        channel_name_, mode_, this, ipc_thread_.message_loop_proxy(), true,
        &shutdown_event_);
  }

  base::Thread* ListenerThread() {
    return overrided_thread_ ? overrided_thread_ : &listener_thread_;
  }

  const base::Thread& ipc_thread() const { return ipc_thread_; }

 private:
  // Called on the listener thread to create the sync channel.
  void OnStart() {
    // Link ipc_thread_, listener_thread_ and channel_ altogether.
    StartThread(&ipc_thread_, MessageLoop::TYPE_IO);
    channel_.reset(CreateChannel());
    channel_created_->Signal();
    Run();
  }

  void OnListenerThreadShutdown1(WaitableEvent* listener_event,
                                 WaitableEvent* ipc_event) {
    // SyncChannel needs to be destructed on the thread that it was created on.
    channel_.reset();

    MessageLoop::current()->RunAllPending();

    ipc_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&Worker::OnIPCThreadShutdown, this,
                              listener_event, ipc_event));
  }

  void OnIPCThreadShutdown(WaitableEvent* listener_event,
                           WaitableEvent* ipc_event) {
    MessageLoop::current()->RunAllPending();
    ipc_event->Signal();

    listener_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&Worker::OnListenerThreadShutdown2, this,
                              listener_event));
  }

  void OnListenerThreadShutdown2(WaitableEvent* listener_event) {
    MessageLoop::current()->RunAllPending();
    listener_event->Signal();
  }

  bool OnMessageReceived(const Message& message) {
    IPC_BEGIN_MESSAGE_MAP(Worker, message)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_Double, OnDoubleDelay)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_AnswerToLife,
                                     OnAnswerDelay)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelNestedTestMsg_String,
                                     OnNestedTestMsg)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void StartThread(base::Thread* thread, MessageLoop::Type type) {
    base::Thread::Options options;
    options.message_loop_type = type;
    thread->StartWithOptions(options);
  }

  scoped_ptr<WaitableEvent> done_;
  scoped_ptr<WaitableEvent> channel_created_;
  std::string channel_name_;
  Channel::Mode mode_;
  scoped_ptr<SyncChannel> channel_;
  base::Thread ipc_thread_;
  base::Thread listener_thread_;
  base::Thread* overrided_thread_;

  base::WaitableEvent shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};


// Starts the test with the given workers.  This function deletes the workers
// when it's done.
void RunTest(std::vector<Worker*> workers) {
  // First we create the workers that are channel servers, or else the other
  // workers' channel initialization might fail because the pipe isn't created..
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() & Channel::MODE_SERVER_FLAG) {
      workers[i]->Start();
      workers[i]->WaitForChannelCreation();
    }
  }

  // now create the clients
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() & Channel::MODE_CLIENT_FLAG)
      workers[i]->Start();
  }

  // wait for all the workers to finish
  for (size_t i = 0; i < workers.size(); ++i)
    workers[i]->done_event()->Wait();

  STLDeleteContainerPointers(workers.begin(), workers.end());
}

}  // namespace

class IPCSyncChannelTest : public testing::Test {
 private:
  MessageLoop message_loop_;
};

//-----------------------------------------------------------------------------

namespace {

class SimpleServer : public Worker {
 public:
  explicit SimpleServer(bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "simpler_server"),
        pump_during_send_(pump_during_send) { }
  void Run() {
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    Done();
  }

  bool pump_during_send_;
};

class SimpleClient : public Worker {
 public:
  SimpleClient() : Worker(Channel::MODE_CLIENT, "simple_client") { }

  void OnAnswer(int* answer) {
    *answer = 42;
    Done();
  }
};

void Simple(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new SimpleServer(pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

}  // namespace

// Tests basic synchronous call
TEST_F(IPCSyncChannelTest, Simple) {
  Simple(false);
  Simple(true);
}

//-----------------------------------------------------------------------------

namespace {

// Worker classes which override how the sync channel is created to use the
// two-step initialization (calling the lightweight constructor and then
// ChannelProxy::Init separately) process.
class TwoStepServer : public Worker {
 public:
  explicit TwoStepServer(bool create_pipe_now)
      : Worker(Channel::MODE_SERVER, "simpler_server"),
        create_pipe_now_(create_pipe_now) { }

  void Run() {
    SendAnswerToLife(false, base::kNoTimeout, true);
    Done();
  }

  virtual SyncChannel* CreateChannel() {
    SyncChannel* channel = new SyncChannel(
        this, ipc_thread().message_loop_proxy(), shutdown_event());
    channel->Init(channel_name(), mode(), create_pipe_now_);
    return channel;
  }

  bool create_pipe_now_;
};

class TwoStepClient : public Worker {
 public:
  TwoStepClient(bool create_pipe_now)
      : Worker(Channel::MODE_CLIENT, "simple_client"),
        create_pipe_now_(create_pipe_now) { }

  void OnAnswer(int* answer) {
    *answer = 42;
    Done();
  }

  virtual SyncChannel* CreateChannel() {
    SyncChannel* channel = new SyncChannel(
        this, ipc_thread().message_loop_proxy(), shutdown_event());
    channel->Init(channel_name(), mode(), create_pipe_now_);
    return channel;
  }

  bool create_pipe_now_;
};

void TwoStep(bool create_server_pipe_now, bool create_client_pipe_now) {
  std::vector<Worker*> workers;
  workers.push_back(new TwoStepServer(create_server_pipe_now));
  workers.push_back(new TwoStepClient(create_client_pipe_now));
  RunTest(workers);
}

}  // namespace

// Tests basic two-step initialization, where you call the lightweight
// constructor then Init.
TEST_F(IPCSyncChannelTest, TwoStepInitialization) {
  TwoStep(false, false);
  TwoStep(false, true);
  TwoStep(true, false);
  TwoStep(true, true);
}


//-----------------------------------------------------------------------------

namespace {

class DelayClient : public Worker {
 public:
  DelayClient() : Worker(Channel::MODE_CLIENT, "delay_client") { }

  void OnAnswerDelay(Message* reply_msg) {
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    Done();
  }
};

void DelayReply(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new SimpleServer(pump_during_send));
  workers.push_back(new DelayClient());
  RunTest(workers);
}

}  // namespace

// Tests that asynchronous replies work
TEST_F(IPCSyncChannelTest, DelayReply) {
  DelayReply(false);
  DelayReply(true);
}

//-----------------------------------------------------------------------------

namespace {

class NoHangServer : public Worker {
 public:
  NoHangServer(WaitableEvent* got_first_reply, bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "no_hang_server"),
        got_first_reply_(got_first_reply),
        pump_during_send_(pump_during_send) { }
  void Run() {
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    got_first_reply_->Signal();

    SendAnswerToLife(pump_during_send_, base::kNoTimeout, false);
    Done();
  }

  WaitableEvent* got_first_reply_;
  bool pump_during_send_;
};

class NoHangClient : public Worker {
 public:
  explicit NoHangClient(WaitableEvent* got_first_reply)
    : Worker(Channel::MODE_CLIENT, "no_hang_client"),
      got_first_reply_(got_first_reply) { }

  virtual void OnAnswerDelay(Message* reply_msg) {
    // Use the DELAY_REPLY macro so that we can force the reply to be sent
    // before this function returns (when the channel will be reset).
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    got_first_reply_->Wait();
    CloseChannel();
    Done();
  }

  WaitableEvent* got_first_reply_;
};

void NoHang(bool pump_during_send) {
  WaitableEvent got_first_reply(false, false);
  std::vector<Worker*> workers;
  workers.push_back(new NoHangServer(&got_first_reply, pump_during_send));
  workers.push_back(new NoHangClient(&got_first_reply));
  RunTest(workers);
}

}  // namespace

// Tests that caller doesn't hang if receiver dies
TEST_F(IPCSyncChannelTest, NoHang) {
  NoHang(false);
  NoHang(true);
}

//-----------------------------------------------------------------------------

namespace {

class UnblockServer : public Worker {
 public:
  UnblockServer(bool pump_during_send, bool delete_during_send)
    : Worker(Channel::MODE_SERVER, "unblock_server"),
      pump_during_send_(pump_during_send),
      delete_during_send_(delete_during_send) { }
  void Run() {
    if (delete_during_send_) {
      // Use custom code since race conditions mean the answer may or may not be
      // available.
      int answer = 0;
      SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
      if (pump_during_send_)
        msg->EnableMessagePumping();
      Send(msg);
    } else {
      SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    }
    Done();
  }

  void OnDoubleDelay(int in, Message* reply_msg) {
    SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, in * 2);
    Send(reply_msg);
    if (delete_during_send_)
      ResetChannel();
  }

  bool pump_during_send_;
  bool delete_during_send_;
};

class UnblockClient : public Worker {
 public:
  explicit UnblockClient(bool pump_during_send)
    : Worker(Channel::MODE_CLIENT, "unblock_client"),
      pump_during_send_(pump_during_send) { }

  void OnAnswer(int* answer) {
    SendDouble(pump_during_send_, true);
    *answer = 42;
    Done();
  }

  bool pump_during_send_;
};

void Unblock(bool server_pump, bool client_pump, bool delete_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new UnblockServer(server_pump, delete_during_send));
  workers.push_back(new UnblockClient(client_pump));
  RunTest(workers);
}

}  // namespace

// Tests that the caller unblocks to answer a sync message from the receiver.
TEST_F(IPCSyncChannelTest, Unblock) {
  Unblock(false, false, false);
  Unblock(false, true, false);
  Unblock(true, false, false);
  Unblock(true, true, false);
}

//-----------------------------------------------------------------------------

// Tests that the the SyncChannel object can be deleted during a Send.
TEST_F(IPCSyncChannelTest, ChannelDeleteDuringSend) {
  Unblock(false, false, true);
  Unblock(false, true, true);
  Unblock(true, false, true);
  Unblock(true, true, true);
}

//-----------------------------------------------------------------------------

namespace {

class RecursiveServer : public Worker {
 public:
  RecursiveServer(bool expected_send_result, bool pump_first, bool pump_second)
      : Worker(Channel::MODE_SERVER, "recursive_server"),
        expected_send_result_(expected_send_result),
        pump_first_(pump_first), pump_second_(pump_second) {}
  void Run() {
    SendDouble(pump_first_, expected_send_result_);
    Done();
  }

  void OnDouble(int in, int* out) {
    *out = in * 2;
    SendAnswerToLife(pump_second_, base::kNoTimeout, expected_send_result_);
  }

  bool expected_send_result_, pump_first_, pump_second_;
};

class RecursiveClient : public Worker {
 public:
  RecursiveClient(bool pump_during_send, bool close_channel)
      : Worker(Channel::MODE_CLIENT, "recursive_client"),
        pump_during_send_(pump_during_send), close_channel_(close_channel) {}

  void OnDoubleDelay(int in, Message* reply_msg) {
    SendDouble(pump_during_send_, !close_channel_);
    if (close_channel_) {
      delete reply_msg;
    } else {
      SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, in * 2);
      Send(reply_msg);
    }
    Done();
  }

  void OnAnswerDelay(Message* reply_msg) {
    if (close_channel_) {
      delete reply_msg;
      CloseChannel();
    } else {
      SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
      Send(reply_msg);
    }
  }

  bool pump_during_send_, close_channel_;
};

void Recursive(
    bool server_pump_first, bool server_pump_second, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(
      new RecursiveServer(true, server_pump_first, server_pump_second));
  workers.push_back(new RecursiveClient(client_pump, false));
  RunTest(workers);
}

}  // namespace

// Tests a server calling Send while another Send is pending.
TEST_F(IPCSyncChannelTest, Recursive) {
  Recursive(false, false, false);
  Recursive(false, false, true);
  Recursive(false, true, false);
  Recursive(false, true, true);
  Recursive(true, false, false);
  Recursive(true, false, true);
  Recursive(true, true, false);
  Recursive(true, true, true);
}

//-----------------------------------------------------------------------------

namespace {

void RecursiveNoHang(
    bool server_pump_first, bool server_pump_second, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(
      new RecursiveServer(false, server_pump_first, server_pump_second));
  workers.push_back(new RecursiveClient(client_pump, true));
  RunTest(workers);
}

}  // namespace

// Tests that if a caller makes a sync call during an existing sync call and
// the receiver dies, neither of the Send() calls hang.
TEST_F(IPCSyncChannelTest, RecursiveNoHang) {
  RecursiveNoHang(false, false, false);
  RecursiveNoHang(false, false, true);
  RecursiveNoHang(false, true, false);
  RecursiveNoHang(false, true, true);
  RecursiveNoHang(true, false, false);
  RecursiveNoHang(true, false, true);
  RecursiveNoHang(true, true, false);
  RecursiveNoHang(true, true, true);
}

//-----------------------------------------------------------------------------

namespace {

class MultipleServer1 : public Worker {
 public:
  explicit MultipleServer1(bool pump_during_send)
    : Worker("test_channel1", Channel::MODE_SERVER),
      pump_during_send_(pump_during_send) { }

  void Run() {
    SendDouble(pump_during_send_, true);
    Done();
  }

  bool pump_during_send_;
};

class MultipleClient1 : public Worker {
 public:
  MultipleClient1(WaitableEvent* client1_msg_received,
                  WaitableEvent* client1_can_reply) :
      Worker("test_channel1", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      client1_can_reply_(client1_can_reply) { }

  void OnDouble(int in, int* out) {
    client1_msg_received_->Signal();
    *out = in * 2;
    client1_can_reply_->Wait();
    Done();
  }

 private:
  WaitableEvent *client1_msg_received_, *client1_can_reply_;
};

class MultipleServer2 : public Worker {
 public:
  MultipleServer2() : Worker("test_channel2", Channel::MODE_SERVER) { }

  void OnAnswer(int* result) {
    *result = 42;
    Done();
  }
};

class MultipleClient2 : public Worker {
 public:
  MultipleClient2(
    WaitableEvent* client1_msg_received, WaitableEvent* client1_can_reply,
    bool pump_during_send)
    : Worker("test_channel2", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      client1_can_reply_(client1_can_reply),
      pump_during_send_(pump_during_send) { }

  void Run() {
    client1_msg_received_->Wait();
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    client1_can_reply_->Signal();
    Done();
  }

 private:
  WaitableEvent *client1_msg_received_, *client1_can_reply_;
  bool pump_during_send_;
};

void Multiple(bool server_pump, bool client_pump) {
  std::vector<Worker*> workers;

  // A shared worker thread so that server1 and server2 run on one thread.
  base::Thread worker_thread("Multiple");
  ASSERT_TRUE(worker_thread.Start());

  // Server1 sends a sync msg to client1, which blocks the reply until
  // server2 (which runs on the same worker thread as server1) responds
  // to a sync msg from client2.
  WaitableEvent client1_msg_received(false, false);
  WaitableEvent client1_can_reply(false, false);

  Worker* worker;

  worker = new MultipleServer2();
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient2(
      &client1_msg_received, &client1_can_reply, client_pump);
  workers.push_back(worker);

  worker = new MultipleServer1(server_pump);
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient1(
      &client1_msg_received, &client1_can_reply);
  workers.push_back(worker);

  RunTest(workers);
}

}  // namespace

// Tests that multiple SyncObjects on the same listener thread can unblock each
// other.
TEST_F(IPCSyncChannelTest, Multiple) {
  Multiple(false, false);
  Multiple(false, true);
  Multiple(true, false);
  Multiple(true, true);
}

//-----------------------------------------------------------------------------

namespace {

// This class provides server side functionality to test the case where
// multiple sync channels are in use on the same thread on the client and
// nested calls are issued.
class QueuedReplyServer : public Worker {
 public:
  QueuedReplyServer(base::Thread* listener_thread,
                    const std::string& channel_name,
                    const std::string& reply_text)
      : Worker(channel_name, Channel::MODE_SERVER),
        reply_text_(reply_text) {
    Worker::OverrideThread(listener_thread);
  }

  virtual void OnNestedTestMsg(Message* reply_msg) {
    VLOG(1) << __FUNCTION__ << " Sending reply: " << reply_text_;
    SyncChannelNestedTestMsg_String::WriteReplyParams(reply_msg, reply_text_);
    Send(reply_msg);
    Done();
  }

 private:
  std::string reply_text_;
};

// The QueuedReplyClient class provides functionality to test the case where
// multiple sync channels are in use on the same thread and they make nested
// sync calls, i.e. while the first channel waits for a response it makes a
// sync call on another channel.
// The callstack should unwind correctly, i.e. the outermost call should
// complete first, and so on.
class QueuedReplyClient : public Worker {
 public:
  QueuedReplyClient(base::Thread* listener_thread,
                    const std::string& channel_name,
                    const std::string& expected_text,
                    bool pump_during_send)
      : Worker(channel_name, Channel::MODE_CLIENT),
        pump_during_send_(pump_during_send),
        expected_text_(expected_text) {
    Worker::OverrideThread(listener_thread);
  }

  virtual void Run() {
    std::string response;
    SyncMessage* msg = new SyncChannelNestedTestMsg_String(&response);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK_EQ(response, expected_text_);

    VLOG(1) << __FUNCTION__ << " Received reply: " << response;
    Done();
  }

 private:
  bool pump_during_send_;
  std::string expected_text_;
};

void QueuedReply(bool client_pump) {
  std::vector<Worker*> workers;

  // A shared worker thread for servers
  base::Thread server_worker_thread("QueuedReply_ServerListener");
  ASSERT_TRUE(server_worker_thread.Start());

  base::Thread client_worker_thread("QueuedReply_ClientListener");
  ASSERT_TRUE(client_worker_thread.Start());

  Worker* worker;

  worker = new QueuedReplyServer(&server_worker_thread,
                                 "QueuedReply_Server1",
                                 "Got first message");
  workers.push_back(worker);

  worker = new QueuedReplyServer(&server_worker_thread,
                                 "QueuedReply_Server2",
                                 "Got second message");
  workers.push_back(worker);

  worker = new QueuedReplyClient(&client_worker_thread,
                                 "QueuedReply_Server1",
                                 "Got first message",
                                 client_pump);
  workers.push_back(worker);

  worker = new QueuedReplyClient(&client_worker_thread,
                                 "QueuedReply_Server2",
                                 "Got second message",
                                 client_pump);
  workers.push_back(worker);

  RunTest(workers);
}

}  // namespace

// While a blocking send is in progress, the listener thread might answer other
// synchronous messages.  This tests that if during the response to another
// message the reply to the original messages comes, it is queued up correctly
// and the original Send is unblocked later.
// We also test that the send call stacks unwind correctly when the channel
// pumps messages while waiting for a response.
TEST_F(IPCSyncChannelTest, QueuedReply) {
  QueuedReply(false);
  QueuedReply(true);
}

//-----------------------------------------------------------------------------

namespace {

class ChattyClient : public Worker {
 public:
  ChattyClient() :
      Worker(Channel::MODE_CLIENT, "chatty_client") { }

  void OnAnswer(int* answer) {
    // The PostMessage limit is 10k.  Send 20% more than that.
    const int kMessageLimit = 10000;
    const int kMessagesToSend = kMessageLimit * 120 / 100;
    for (int i = 0; i < kMessagesToSend; ++i) {
      if (!SendDouble(false, true))
        break;
    }
    *answer = 42;
    Done();
  }
};

void ChattyServer(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new UnblockServer(pump_during_send, false));
  workers.push_back(new ChattyClient());
  RunTest(workers);
}

}  // namespace

// Tests http://b/1093251 - that sending lots of sync messages while
// the receiver is waiting for a sync reply does not overflow the PostMessage
// queue.
TEST_F(IPCSyncChannelTest, ChattyServer) {
  ChattyServer(false);
  ChattyServer(true);
}

//------------------------------------------------------------------------------

namespace {

class TimeoutServer : public Worker {
 public:
  TimeoutServer(int timeout_ms,
                std::vector<bool> timeout_seq,
                bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "timeout_server"),
        timeout_ms_(timeout_ms),
        timeout_seq_(timeout_seq),
        pump_during_send_(pump_during_send) {
  }

  void Run() {
    for (std::vector<bool>::const_iterator iter = timeout_seq_.begin();
         iter != timeout_seq_.end(); ++iter) {
      SendAnswerToLife(pump_during_send_, timeout_ms_, !*iter);
    }
    Done();
  }

 private:
  int timeout_ms_;
  std::vector<bool> timeout_seq_;
  bool pump_during_send_;
};

class UnresponsiveClient : public Worker {
 public:
  explicit UnresponsiveClient(std::vector<bool> timeout_seq)
      : Worker(Channel::MODE_CLIENT, "unresponsive_client"),
        timeout_seq_(timeout_seq) {
  }

  void OnAnswerDelay(Message* reply_msg) {
    DCHECK(!timeout_seq_.empty());
    if (!timeout_seq_[0]) {
      SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
      Send(reply_msg);
    } else {
      // Don't reply.
      delete reply_msg;
    }
    timeout_seq_.erase(timeout_seq_.begin());
    if (timeout_seq_.empty())
      Done();
  }

 private:
  // Whether we should time-out or respond to the various messages we receive.
  std::vector<bool> timeout_seq_;
};

void SendWithTimeoutOK(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(5000, timeout_seq, pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

void SendWithTimeoutTimeout(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(100, timeout_seq, pump_during_send));
  workers.push_back(new UnresponsiveClient(timeout_seq));
  RunTest(workers);
}

void SendWithTimeoutMixedOKAndTimeout(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(100, timeout_seq, pump_during_send));
  workers.push_back(new UnresponsiveClient(timeout_seq));
  RunTest(workers);
}

}  // namespace

// Tests that SendWithTimeout does not time-out if the response comes back fast
// enough.
TEST_F(IPCSyncChannelTest, SendWithTimeoutOK) {
  SendWithTimeoutOK(false);
  SendWithTimeoutOK(true);
}

// Tests that SendWithTimeout does time-out.
TEST_F(IPCSyncChannelTest, SendWithTimeoutTimeout) {
  SendWithTimeoutTimeout(false);
  SendWithTimeoutTimeout(true);
}

// Sends some message that time-out and some that succeed.
// Crashes flakily, http://crbug.com/70075.
TEST_F(IPCSyncChannelTest, DISABLED_SendWithTimeoutMixedOKAndTimeout) {
  SendWithTimeoutMixedOKAndTimeout(false);
  SendWithTimeoutMixedOKAndTimeout(true);
}

//------------------------------------------------------------------------------

namespace {

void NestedCallback(Worker* server) {
  // Sleep a bit so that we wake up after the reply has been received.
  base::PlatformThread::Sleep(250);
  server->SendAnswerToLife(true, base::kNoTimeout, true);
}

bool timeout_occurred = false;

void TimeoutCallback() {
  timeout_occurred = true;
}

class DoneEventRaceServer : public Worker {
 public:
  DoneEventRaceServer()
      : Worker(Channel::MODE_SERVER, "done_event_race_server") { }

  void Run() {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(&NestedCallback, this));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&TimeoutCallback), 9000);
    // Even though we have a timeout on the Send, it will succeed since for this
    // bug, the reply message comes back and is deserialized, however the done
    // event wasn't set.  So we indirectly use the timeout task to notice if a
    // timeout occurred.
    SendAnswerToLife(true, 10000, true);
    DCHECK(!timeout_occurred);
    Done();
  }
};

}  // namespace

// Tests http://b/1474092 - that if after the done_event is set but before
// OnObjectSignaled is called another message is sent out, then after its
// reply comes back OnObjectSignaled will be called for the first message.
TEST_F(IPCSyncChannelTest, DoneEventRace) {
  std::vector<Worker*> workers;
  workers.push_back(new DoneEventRaceServer());
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

//-----------------------------------------------------------------------------

namespace {

class TestSyncMessageFilter : public SyncMessageFilter {
 public:
  TestSyncMessageFilter(base::WaitableEvent* shutdown_event, Worker* worker)
      : SyncMessageFilter(shutdown_event),
        worker_(worker),
        thread_("helper_thread") {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_DEFAULT;
    thread_.StartWithOptions(options);
  }

  virtual void OnFilterAdded(Channel* channel) {
    SyncMessageFilter::OnFilterAdded(channel);
    thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&TestSyncMessageFilter::SendMessageOnHelperThread, this));
  }

  void SendMessageOnHelperThread() {
    int answer = 0;
    bool result = Send(new SyncChannelTestMsg_AnswerToLife(&answer));
    DCHECK(result);
    DCHECK_EQ(answer, 42);

    worker_->Done();
  }

  Worker* worker_;
  base::Thread thread_;
};

class SyncMessageFilterServer : public Worker {
 public:
  SyncMessageFilterServer()
      : Worker(Channel::MODE_SERVER, "sync_message_filter_server") {
    filter_ = new TestSyncMessageFilter(shutdown_event(), this);
  }

  void Run() {
    channel()->AddFilter(filter_.get());
  }

  scoped_refptr<TestSyncMessageFilter> filter_;
};

// This class provides functionality to test the case that a Send on the sync
// channel does not crash after the channel has been closed.
class ServerSendAfterClose : public Worker {
 public:
  ServerSendAfterClose()
     : Worker(Channel::MODE_SERVER, "simpler_server"),
       send_result_(true) {
  }

  bool SendDummy() {
    ListenerThread()->message_loop()->PostTask(
        FROM_HERE, base::IgnoreReturn<bool>(
            base::Bind(&ServerSendAfterClose::Send, this,
                       new SyncChannelTestMsg_NoArgs)));
    return true;
  }

  bool send_result() const {
    return send_result_;
  }

 private:
  virtual void Run() {
    CloseChannel();
    Done();
  }

  bool Send(Message* msg) {
    send_result_ = Worker::Send(msg);
    Done();
    return send_result_;
  }

  bool send_result_;
};

}  // namespace

// Tests basic synchronous call
TEST_F(IPCSyncChannelTest, SyncMessageFilter) {
  std::vector<Worker*> workers;
  workers.push_back(new SyncMessageFilterServer());
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

// Test the case when the channel is closed and a Send is attempted after that.
TEST_F(IPCSyncChannelTest, SendAfterClose) {
  ServerSendAfterClose server;
  server.Start();

  server.done_event()->Wait();
  server.done_event()->Reset();

  server.SendDummy();
  server.done_event()->Wait();

  EXPECT_FALSE(server.send_result());
}

//-----------------------------------------------------------------------------

namespace {

class RestrictedDispatchServer : public Worker {
 public:
  RestrictedDispatchServer(WaitableEvent* sent_ping_event)
      : Worker("restricted_channel", Channel::MODE_SERVER),
        sent_ping_event_(sent_ping_event) { }

  void OnDoPing(int ping) {
    // Send an asynchronous message that unblocks the caller.
    Message* msg = new SyncChannelTestMsg_Ping(ping);
    msg->set_unblock(true);
    Send(msg);
    // Signal the event after the message has been sent on the channel, on the
    // IPC thread.
    ipc_thread().message_loop()->PostTask(
        FROM_HERE, base::Bind(&RestrictedDispatchServer::OnPingSent, this));
  }

  base::Thread* ListenerThread() { return Worker::ListenerThread(); }

 private:
  bool OnMessageReceived(const Message& message) {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchServer, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs, OnNoArgs)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Done, Done)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnPingSent() {
    sent_ping_event_->Signal();
  }

  void OnNoArgs() { }
  WaitableEvent* sent_ping_event_;
};

class NonRestrictedDispatchServer : public Worker {
 public:
  NonRestrictedDispatchServer()
      : Worker("non_restricted_channel", Channel::MODE_SERVER) {}

 private:
  bool OnMessageReceived(const Message& message) {
    IPC_BEGIN_MESSAGE_MAP(NonRestrictedDispatchServer, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs, OnNoArgs)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Done, Done)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnNoArgs() { }
};

class RestrictedDispatchClient : public Worker {
 public:
  RestrictedDispatchClient(WaitableEvent* sent_ping_event,
                           RestrictedDispatchServer* server,
                           int* success)
      : Worker("restricted_channel", Channel::MODE_CLIENT),
        ping_(0),
        server_(server),
        success_(success),
        sent_ping_event_(sent_ping_event) {}

  void Run() {
    // Incoming messages from our channel should only be dispatched when we
    // send a message on that same channel.
    channel()->SetRestrictDispatchToSameChannel(true);

    server_->ListenerThread()->message_loop()->PostTask(
        FROM_HERE, base::Bind(&RestrictedDispatchServer::OnDoPing, server_, 1));
    sent_ping_event_->Wait();
    Send(new SyncChannelTestMsg_NoArgs);
    if (ping_ == 1)
      ++*success_;
    else
      LOG(ERROR) << "Send failed to dispatch incoming message on same channel";

    scoped_ptr<SyncChannel> non_restricted_channel(new SyncChannel(
        "non_restricted_channel", Channel::MODE_CLIENT, this,
        ipc_thread().message_loop_proxy(), true, shutdown_event()));

    server_->ListenerThread()->message_loop()->PostTask(
        FROM_HERE, base::Bind(&RestrictedDispatchServer::OnDoPing, server_, 2));
    sent_ping_event_->Wait();
    // Check that the incoming message is *not* dispatched when sending on the
    // non restricted channel.
    // TODO(piman): there is a possibility of a false positive race condition
    // here, if the message that was posted on the server-side end of the pipe
    // is not visible yet on the client side, but I don't know how to solve this
    // without hooking into the internals of SyncChannel. I haven't seen it in
    // practice (i.e. not setting SetRestrictDispatchToSameChannel does cause
    // the following to fail).
    non_restricted_channel->Send(new SyncChannelTestMsg_NoArgs);
    if (ping_ == 1)
      ++*success_;
    else
      LOG(ERROR) << "Send dispatched message from restricted channel";

    Send(new SyncChannelTestMsg_NoArgs);
    if (ping_ == 2)
      ++*success_;
    else
      LOG(ERROR) << "Send failed to dispatch incoming message on same channel";

    non_restricted_channel->Send(new SyncChannelTestMsg_Done);
    non_restricted_channel.reset();
    Send(new SyncChannelTestMsg_Done);
    Done();
  }

 private:
  bool OnMessageReceived(const Message& message) {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchClient, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Ping, OnPing)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnPing(int ping) {
    ping_ = ping;
  }

  int ping_;
  RestrictedDispatchServer* server_;
  int* success_;
  WaitableEvent* sent_ping_event_;
};

}  // namespace

TEST_F(IPCSyncChannelTest, RestrictedDispatch) {
  WaitableEvent sent_ping_event(false, false);

  RestrictedDispatchServer* server =
      new RestrictedDispatchServer(&sent_ping_event);
  int success = 0;
  std::vector<Worker*> workers;
  workers.push_back(new NonRestrictedDispatchServer);
  workers.push_back(server);
  workers.push_back(
      new RestrictedDispatchClient(&sent_ping_event, server, &success));
  RunTest(workers);
  EXPECT_EQ(3, success);
}

}  // namespace IPC
