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

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/perftimer.h"
#include "base/pickle.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_multiprocess_test.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_test_base.h"

namespace {

// This test times the roundtrip IPC message cycle.
//
// TODO(brettw): Make this test run by default.

class IPCChannelPerfTest : public IPCTestBase {
};

// This class simply collects stats about abstract "events" (each of which has a
// start time and an end time).
class EventTimeTracker {
 public:
  explicit EventTimeTracker(const char* name)
      : name_(name),
        count_(0) {
  }

  void AddEvent(const base::TimeTicks& start, const base::TimeTicks& end) {
    DCHECK(end >= start);
    count_++;
    base::TimeDelta duration = end - start;
    total_duration_ += duration;
    max_duration_ = std::max(max_duration_, duration);
  }

  void ShowResults() const {
    VLOG(1) << name_ << " count: " << count_;
    VLOG(1) << name_ << " total duration: "
            << total_duration_.InMillisecondsF() << " ms";
    VLOG(1) << name_ << " average duration: "
            << (total_duration_.InMillisecondsF() / static_cast<double>(count_))
            << " ms";
    VLOG(1) << name_ << " maximum duration: "
            << max_duration_.InMillisecondsF() << " ms";
  }

  void Reset() {
    count_ = 0;
    total_duration_ = base::TimeDelta();
    max_duration_ = base::TimeDelta();
  }

 private:
  const std::string name_;

  uint64 count_;
  base::TimeDelta total_duration_;
  base::TimeDelta max_duration_;

  DISALLOW_COPY_AND_ASSIGN(EventTimeTracker);
};

// This channel listener just replies to all messages with the exact same
// message. It assumes each message has one string parameter. When the string
// "quit" is sent, it will exit.
class ChannelReflectorListener : public IPC::Listener {
 public:
  explicit ChannelReflectorListener(IPC::Channel* channel)
      : channel_(channel),
        latency_tracker_("Client messages") {
    VLOG(1) << "Client listener up";
  }

  ~ChannelReflectorListener() {
    VLOG(1) << "Client listener down";
    latency_tracker_.ShowResults();
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    PickleIterator iter(message);
    int64 time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    std::string payload;
    EXPECT_TRUE(iter.ReadString(&payload));

    // Include message deserialization in latency.
    base::TimeTicks now = base::TimeTicks::Now();

    if (payload == "hello") {
      latency_tracker_.Reset();
    } else if (payload == "quit") {
      latency_tracker_.ShowResults();
      MessageLoop::current()->QuitWhenIdle();
      return true;
    } else {
      // Don't track hello and quit messages.
      latency_tracker_.AddEvent(
          base::TimeTicks::FromInternalValue(time_internal), now);
    }

    IPC::Message* msg = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(msgid);
    msg->WriteString(payload);
    channel_->Send(msg);
    return true;
  }

 private:
  IPC::Channel* channel_;
  EventTimeTracker latency_tracker_;
};

class ChannelPerfListener : public IPC::Listener {
 public:
  ChannelPerfListener(IPC::Channel* channel)
      : channel_(channel),
        msg_count_(0),
        msg_size_(0),
        count_down_(0),
        latency_tracker_("Server messages") {
    VLOG(1) << "Server listener up";
  }

  ~ChannelPerfListener() {
    VLOG(1) << "Server listener down";
  }

  // Call this before running the message loop.
  void SetTestParams(int msg_count, size_t msg_size) {
    DCHECK_EQ(0, count_down_);
    msg_count_ = msg_count;
    msg_size_ = msg_size;
    count_down_ = msg_count_;
    payload_ = std::string(msg_size_, 'a');
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    PickleIterator iter(message);
    int64 time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    std::string reflected_payload;
    EXPECT_TRUE(iter.ReadString(&reflected_payload));

    // Include message deserialization in latency.
    base::TimeTicks now = base::TimeTicks::Now();

    if (reflected_payload == "hello") {
      // Start timing on hello.
      latency_tracker_.Reset();
      DCHECK(!perf_logger_.get());
      std::string test_name = base::StringPrintf(
          "IPC_Perf_%dx_%u", msg_count_, static_cast<unsigned>(msg_size_));
      perf_logger_.reset(new PerfTimeLogger(test_name.c_str()));
    } else {
      DCHECK_EQ(payload_.size(), reflected_payload.size());

      latency_tracker_.AddEvent(
          base::TimeTicks::FromInternalValue(time_internal), now);

      CHECK(count_down_ > 0);
      count_down_--;
      if (count_down_ == 0) {
        perf_logger_.reset();  // Stop the perf timer now.
        latency_tracker_.ShowResults();
        MessageLoop::current()->QuitWhenIdle();
        return true;
      }
    }

    IPC::Message* msg = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(count_down_);
    msg->WriteString(payload_);
    channel_->Send(msg);
    return true;
  }

 private:
  IPC::Channel* channel_;
  int msg_count_;
  size_t msg_size_;

  int count_down_;
  std::string payload_;
  EventTimeTracker latency_tracker_;
  scoped_ptr<PerfTimeLogger> perf_logger_;
};

TEST_F(IPCChannelPerfTest, Performance) {
  // Setup IPC channel.
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_SERVER, NULL);
  ChannelPerfListener perf_listener(&chan);
  chan.set_listener(&perf_listener);
  ASSERT_TRUE(chan.Connect());

  base::ProcessHandle process_handle = SpawnChild(TEST_REFLECTOR, &chan);
  ASSERT_TRUE(process_handle);

  const size_t kMsgSizeBase = 12;
  const int kMsgSizeMaxExp = 5;
  int msg_count = 100000;
  size_t msg_size = kMsgSizeBase;
  for (int i = 1; i <= kMsgSizeMaxExp; i++) {
    perf_listener.SetTestParams(msg_count, msg_size);

    // This initial message will kick-start the ping-pong of messages.
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    message->WriteInt(-1);
    message->WriteString("hello");
    chan.Send(message);

    // Run message loop.
    MessageLoop::current()->Run();

    msg_size *= kMsgSizeBase;
  }

  // Send quit message.
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
  message->WriteInt(-1);
  message->WriteString("quit");
  chan.Send(message);

  // Clean up child process.
  EXPECT_TRUE(base::WaitForSingleProcess(process_handle,
                                         base::TimeDelta::FromSeconds(5)));
  base::CloseProcessHandle(process_handle);
}

// This message loop bounces all messages back to the sender.
MULTIPROCESS_IPC_TEST_MAIN(RunReflector) {
  MessageLoopForIO main_message_loop;
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_CLIENT, NULL);
  ChannelReflectorListener channel_reflector_listener(&chan);
  chan.set_listener(&channel_reflector_listener);
  CHECK(chan.Connect());

  MessageLoop::current()->Run();
  return 0;
}

}  // namespace
