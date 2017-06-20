// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/perf_time_logger.h"
#include "base/test/test_io_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_test.mojom.h"
#include "ipc/ipc_test_base.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {
namespace {

// This class simply collects stats about abstract "events" (each of which has a
// start time and an end time).
class EventTimeTracker {
 public:
  explicit EventTimeTracker(const char* name) : name_(name), count_(0) {}

  void AddEvent(const base::TimeTicks& start, const base::TimeTicks& end) {
    DCHECK(end >= start);
    count_++;
    base::TimeDelta duration = end - start;
    total_duration_ += duration;
    max_duration_ = std::max(max_duration_, duration);
  }

  void ShowResults() const {
    VLOG(1) << name_ << " count: " << count_;
    VLOG(1) << name_ << " total duration: " << total_duration_.InMillisecondsF()
            << " ms";
    VLOG(1) << name_ << " average duration: "
            << (total_duration_.InMillisecondsF() / static_cast<double>(count_))
            << " ms";
    VLOG(1) << name_ << " maximum duration: " << max_duration_.InMillisecondsF()
            << " ms";
  }

  void Reset() {
    count_ = 0;
    total_duration_ = base::TimeDelta();
    max_duration_ = base::TimeDelta();
  }

 private:
  const std::string name_;

  uint64_t count_;
  base::TimeDelta total_duration_;
  base::TimeDelta max_duration_;

  DISALLOW_COPY_AND_ASSIGN(EventTimeTracker);
};

class PerformanceChannelListener : public Listener {
 public:
  explicit PerformanceChannelListener(const std::string& label)
      : label_(label),
        sender_(NULL),
        msg_count_(0),
        msg_size_(0),
        count_down_(0),
        latency_tracker_("Server messages") {
    VLOG(1) << "Server listener up";
  }

  ~PerformanceChannelListener() override { VLOG(1) << "Server listener down"; }

  void Init(Sender* sender) {
    DCHECK(!sender_);
    sender_ = sender;
  }

  // Call this before running the message loop.
  void SetTestParams(int msg_count, size_t msg_size) {
    DCHECK_EQ(0, count_down_);
    msg_count_ = msg_count;
    msg_size_ = msg_size;
    count_down_ = msg_count_;
    payload_ = std::string(msg_size_, 'a');
  }

  bool OnMessageReceived(const Message& message) override {
    CHECK(sender_);

    base::PickleIterator iter(message);
    int64_t time_internal;
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
      std::string test_name =
          base::StringPrintf("IPC_%s_Perf_%dx_%u", label_.c_str(), msg_count_,
                             static_cast<unsigned>(msg_size_));
      perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));
    } else {
      DCHECK_EQ(payload_.size(), reflected_payload.size());

      latency_tracker_.AddEvent(
          base::TimeTicks::FromInternalValue(time_internal), now);

      CHECK(count_down_ > 0);
      count_down_--;
      if (count_down_ == 0) {
        perf_logger_.reset();  // Stop the perf timer now.
        latency_tracker_.ShowResults();
        base::MessageLoop::current()->QuitWhenIdle();
        return true;
      }
    }

    Message* msg = new Message(0, 2, Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(count_down_);
    msg->WriteString(payload_);
    sender_->Send(msg);
    return true;
  }

 private:
  std::string label_;
  Sender* sender_;
  int msg_count_;
  size_t msg_size_;

  int count_down_;
  std::string payload_;
  EventTimeTracker latency_tracker_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;
};

// This channel listener just replies to all messages with the exact same
// message. It assumes each message has one string parameter. When the string
// "quit" is sent, it will exit.
class ChannelReflectorListener : public Listener {
 public:
  ChannelReflectorListener()
      : channel_(NULL), latency_tracker_("Client messages") {
    VLOG(1) << "Client listener up";
  }

  ~ChannelReflectorListener() override {
    VLOG(1) << "Client listener down";
    latency_tracker_.ShowResults();
  }

  void Init(Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  bool OnMessageReceived(const Message& message) override {
    CHECK(channel_);

    base::PickleIterator iter(message);
    int64_t time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    base::StringPiece payload;
    EXPECT_TRUE(iter.ReadStringPiece(&payload));

    // Include message deserialization in latency.
    base::TimeTicks now = base::TimeTicks::Now();

    if (payload == "hello") {
      latency_tracker_.Reset();
    } else if (payload == "quit") {
      latency_tracker_.ShowResults();
      base::MessageLoop::current()->QuitWhenIdle();
      return true;
    } else {
      // Don't track hello and quit messages.
      latency_tracker_.AddEvent(
          base::TimeTicks::FromInternalValue(time_internal), now);
    }

    Message* msg = new Message(0, 2, Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(msgid);
    msg->WriteString(payload);
    channel_->Send(msg);
    return true;
  }

 private:
  Channel* channel_;
  EventTimeTracker latency_tracker_;
};

// This class locks the current thread to a particular CPU core. This is
// important because otherwise the different threads and processes of these
// tests end up on different CPU cores which means that all of the cores are
// lightly loaded so the OS (Windows and Linux) fails to ramp up the CPU
// frequency, leading to unpredictable and often poor performance.
class LockThreadAffinity {
 public:
  explicit LockThreadAffinity(int cpu_number) : affinity_set_ok_(false) {
#if defined(OS_WIN)
    const DWORD_PTR thread_mask = static_cast<DWORD_PTR>(1) << cpu_number;
    old_affinity_ = SetThreadAffinityMask(GetCurrentThread(), thread_mask);
    affinity_set_ok_ = old_affinity_ != 0;
#elif defined(OS_LINUX)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_number, &cpuset);
    auto get_result = sched_getaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
    DCHECK_EQ(0, get_result);
    auto set_result = sched_setaffinity(0, sizeof(cpuset), &cpuset);
    // Check for get_result failure, even though it should always succeed.
    affinity_set_ok_ = (set_result == 0) && (get_result == 0);
#endif
    if (!affinity_set_ok_)
      LOG(WARNING) << "Failed to set thread affinity to CPU " << cpu_number;
  }

  ~LockThreadAffinity() {
    if (!affinity_set_ok_)
      return;
#if defined(OS_WIN)
    auto set_result = SetThreadAffinityMask(GetCurrentThread(), old_affinity_);
    DCHECK_NE(0u, set_result);
#elif defined(OS_LINUX)
    auto set_result = sched_setaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
    DCHECK_EQ(0, set_result);
#endif
  }

 private:
  bool affinity_set_ok_;
#if defined(OS_WIN)
  DWORD_PTR old_affinity_;
#elif defined(OS_LINUX)
  cpu_set_t old_cpuset_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LockThreadAffinity);
};

class PingPongTestParams {
 public:
  PingPongTestParams(size_t size, int count)
      : message_size_(size), message_count_(count) {}

  size_t message_size() const { return message_size_; }
  int message_count() const { return message_count_; }

 private:
  size_t message_size_;
  int message_count_;
};

std::vector<PingPongTestParams> GetDefaultTestParams() {
// Test several sizes. We use 12^N for message size, and limit the message
// count to keep the test duration reasonable.
#ifdef NDEBUG
  const int kMultiplier = 100;
#else
  // Debug builds on Windows run these tests orders of magnitude more slowly.
  const int kMultiplier = 1;
#endif
  std::vector<PingPongTestParams> list;
  list.push_back(PingPongTestParams(12, 500 * kMultiplier));
  list.push_back(PingPongTestParams(144, 500 * kMultiplier));
  list.push_back(PingPongTestParams(1728, 500 * kMultiplier));
  list.push_back(PingPongTestParams(20736, 120 * kMultiplier));
  list.push_back(PingPongTestParams(248832, 10 * kMultiplier));
  return list;
}

// Avoid core 0 due to conflicts with Intel's Power Gadget.
// Setting thread affinity will fail harmlessly on single/dual core machines.
const int kSharedCore = 2;

class MojoChannelPerfTest : public IPCChannelMojoTestBase {
 public:
  MojoChannelPerfTest() = default;
  ~MojoChannelPerfTest() override = default;

  void RunTestChannelPingPong() {
    Init("MojoPerfTestClient");

    // Set up IPC channel and start client.
    PerformanceChannelListener listener("Channel");
    CreateChannel(&listener);
    listener.Init(channel());
    ASSERT_TRUE(ConnectChannel());

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    for (size_t i = 0; i < params.size(); i++) {
      listener.SetTestParams(params[i].message_count(),
                             params[i].message_size());

      // This initial message will kick-start the ping-pong of messages.
      Message* message = new Message(0, 2, Message::PRIORITY_NORMAL);
      message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
      message->WriteInt(-1);
      message->WriteString("hello");
      sender()->Send(message);

      // Run message loop.
      base::RunLoop().Run();
    }

    // Send quit message.
    Message* message = new Message(0, 2, Message::PRIORITY_NORMAL);
    message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    message->WriteInt(-1);
    message->WriteString("quit");
    sender()->Send(message);

    EXPECT_TRUE(WaitForClientShutdown());
    DestroyChannel();
  }

  void RunTestChannelProxyPingPong() {
    io_thread_.reset(new base::TestIOThread(base::TestIOThread::kAutoStart));

    Init("MojoPerfTestClient");

    // Set up IPC channel and start client.
    PerformanceChannelListener listener("ChannelProxy");
    auto channel_proxy = IPC::ChannelProxy::Create(
        TakeHandle().release(), IPC::Channel::MODE_SERVER, &listener,
        io_thread_->task_runner());
    listener.Init(channel_proxy.get());

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    for (size_t i = 0; i < params.size(); i++) {
      listener.SetTestParams(params[i].message_count(),
                             params[i].message_size());

      // This initial message will kick-start the ping-pong of messages.
      Message* message = new Message(0, 2, Message::PRIORITY_NORMAL);
      message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
      message->WriteInt(-1);
      message->WriteString("hello");
      channel_proxy->Send(message);

      // Run message loop.
      base::RunLoop().Run();
    }

    // Send quit message.
    Message* message = new Message(0, 2, Message::PRIORITY_NORMAL);
    message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    message->WriteInt(-1);
    message->WriteString("quit");
    channel_proxy->Send(message);

    EXPECT_TRUE(WaitForClientShutdown());
    channel_proxy.reset();

    io_thread_.reset();
  }

  scoped_refptr<base::TaskRunner> io_task_runner() {
    if (io_thread_)
      return io_thread_->task_runner();
    return base::ThreadTaskRunnerHandle::Get();
  }

 private:
  std::unique_ptr<base::TestIOThread> io_thread_;
};

TEST_F(MojoChannelPerfTest, ChannelPingPong) {
  RunTestChannelPingPong();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(MojoChannelPerfTest, ChannelProxyPingPong) {
  RunTestChannelProxyPingPong();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Test to see how many channels we can create.
TEST_F(MojoChannelPerfTest, DISABLED_MaxChannelCount) {
#if defined(OS_POSIX)
  LOG(INFO) << "base::GetMaxFds " << base::GetMaxFds();
  base::SetFdLimit(20000);
#endif

  std::vector<mojo::edk::PlatformChannelPair*> channels;
  for (size_t i = 0; i < 10000; ++i) {
    LOG(INFO) << "channels size: " << channels.size();
    channels.push_back(new mojo::edk::PlatformChannelPair());
  }
}

class MojoPerfTestClient {
 public:
  MojoPerfTestClient() : listener_(new ChannelReflectorListener()) {
    mojo::edk::test::MultiprocessTestHelper::ChildSetup();
  }

  ~MojoPerfTestClient() = default;

  int Run(MojoHandle handle) {
    handle_ = mojo::MakeScopedHandle(mojo::MessagePipeHandle(handle));
    LockThreadAffinity thread_locker(kSharedCore);
    std::unique_ptr<Channel> channel = ChannelMojo::Create(
        std::move(handle_), Channel::MODE_CLIENT, listener_.get());
    listener_->Init(channel.get());
    CHECK(channel->Connect());

    base::RunLoop().Run();
    return 0;
  }

 private:
  base::MessageLoopForIO main_message_loop_;
  std::unique_ptr<ChannelReflectorListener> listener_;
  std::unique_ptr<Channel> channel_;
  mojo::ScopedMessagePipeHandle handle_;
};

MULTIPROCESS_TEST_MAIN(MojoPerfTestClientTestChildMain) {
  MojoPerfTestClient client;
  int rv = mojo::edk::test::MultiprocessTestHelper::RunClientMain(
      base::Bind(&MojoPerfTestClient::Run, base::Unretained(&client)),
      true /* pass_pipe_ownership_to_main */);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return rv;
}

class ReflectorImpl : public IPC::mojom::Reflector {
 public:
  explicit ReflectorImpl(mojo::ScopedMessagePipeHandle handle)
      : binding_(this, IPC::mojom::ReflectorRequest(std::move(handle))) {}
  ~ReflectorImpl() override {
    ignore_result(binding_.Unbind().PassMessagePipe().release());
  }

 private:
  // IPC::mojom::Reflector:
  void Ping(const std::string& value, PingCallback callback) override {
    std::move(callback).Run(value);
  }

  void Quit() override { base::MessageLoop::current()->QuitWhenIdle(); }

  mojo::Binding<IPC::mojom::Reflector> binding_;
};

class MojoInterfacePerfTest : public mojo::edk::test::MojoTestBase {
 public:
  MojoInterfacePerfTest() : message_count_(0), count_down_(0) {}

 protected:
  void RunPingPongServer(MojoHandle mp, const std::string& label) {
    label_ = label;

    mojo::MessagePipeHandle mp_handle(mp);
    mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);
    ping_receiver_.Bind(IPC::mojom::ReflectorPtrInfo(std::move(scoped_mp), 0u));

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    for (size_t i = 0; i < params.size(); i++) {
      ping_receiver_->Ping("hello", base::Bind(&MojoInterfacePerfTest::OnPong,
                                               base::Unretained(this)));
      message_count_ = count_down_ = params[i].message_count();
      payload_ = std::string(params[i].message_size(), 'a');

      base::RunLoop().Run();
    }

    ping_receiver_->Quit();

    ignore_result(ping_receiver_.PassInterface().PassHandle().release());
  }

  void OnPong(const std::string& value) {
    if (value == "hello") {
      DCHECK(!perf_logger_.get());
      std::string test_name =
          base::StringPrintf("IPC_%s_Perf_%dx_%zu", label_.c_str(),
                             message_count_, payload_.size());
      perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));
    } else {
      DCHECK_EQ(payload_.size(), value.size());

      CHECK(count_down_ > 0);
      count_down_--;
      if (count_down_ == 0) {
        perf_logger_.reset();
        base::MessageLoop::current()->QuitWhenIdle();
        return;
      }
    }

    ping_receiver_->Ping(payload_, base::Bind(&MojoInterfacePerfTest::OnPong,
                                              base::Unretained(this)));
  }

  static int RunPingPongClient(MojoHandle mp) {
    mojo::MessagePipeHandle mp_handle(mp);
    mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);

    // In single process mode, this is running in a task and by default other
    // tasks (in particular, the binding) won't run. To keep the single process
    // and multi-process code paths the same, enable nestable tasks.
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());

    LockThreadAffinity thread_locker(kSharedCore);
    ReflectorImpl impl(std::move(scoped_mp));
    base::RunLoop().Run();
    return 0;
  }

 private:
  int message_count_;
  int count_down_;
  std::string label_;
  std::string payload_;
  IPC::mojom::ReflectorPtr ping_receiver_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;

  DISALLOW_COPY_AND_ASSIGN(MojoInterfacePerfTest);
};

DEFINE_TEST_CLIENT_WITH_PIPE(PingPongClient, MojoInterfacePerfTest, h) {
  base::MessageLoop main_message_loop;
  return RunPingPongClient(h);
}

// Similar to MojoChannelPerfTest above, but uses a Mojo interface instead of
// raw IPC::Messages.
TEST_F(MojoInterfacePerfTest, MultiprocessPingPong) {
  RunTestClient("PingPongClient", [&](MojoHandle h) {
    base::MessageLoop main_message_loop;
    RunPingPongServer(h, "Multiprocess");
  });
}

// A single process version of the above test.
TEST_F(MojoInterfacePerfTest, SingleProcessMultiThreadPingPong) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::Thread client_thread("PingPongClient");
  client_thread.Start();
  client_thread.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&RunPingPongClient), client_handle));

  base::MessageLoop main_message_loop;
  RunPingPongServer(server_handle, "SingleProcess");
}

TEST_F(MojoInterfacePerfTest, SingleProcessSingleThreadPingPong) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::MessageLoop main_message_loop;
  mojo::MessagePipeHandle mp_handle(client_handle);
  mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);
  LockThreadAffinity thread_locker(kSharedCore);
  ReflectorImpl impl(std::move(scoped_mp));

  RunPingPongServer(server_handle, "SingleProcess");
}

class CallbackPerfTest : public testing::Test {
 public:
  CallbackPerfTest()
      : client_thread_("PingPongClient"), message_count_(0), count_down_(0) {}

 protected:
  void RunMultiThreadPingPongServer() {
    client_thread_.Start();

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    for (size_t i = 0; i < params.size(); i++) {
      std::string hello("hello");
      client_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&CallbackPerfTest::Ping, base::Unretained(this), hello));
      message_count_ = count_down_ = params[i].message_count();
      payload_ = std::string(params[i].message_size(), 'a');

      base::RunLoop().Run();
    }
  }

  void Ping(const std::string& value) {
    main_message_loop_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&CallbackPerfTest::OnPong, base::Unretained(this), value));
  }

  void OnPong(const std::string& value) {
    if (value == "hello") {
      DCHECK(!perf_logger_.get());
      std::string test_name =
          base::StringPrintf("Callback_MultiProcess_Perf_%dx_%zu",
                             message_count_, payload_.size());
      perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));
    } else {
      DCHECK_EQ(payload_.size(), value.size());

      CHECK(count_down_ > 0);
      count_down_--;
      if (count_down_ == 0) {
        perf_logger_.reset();
        base::MessageLoop::current()->QuitWhenIdle();
        return;
      }
    }

    client_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&CallbackPerfTest::Ping, base::Unretained(this), payload_));
  }

  void RunSingleThreadNoPostTaskPingPongServer() {
    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    base::Callback<void(const std::string&,
                        const base::Callback<void(const std::string&)>&)>
        ping = base::Bind(&CallbackPerfTest::SingleThreadPingNoPostTask,
                          base::Unretained(this));
    for (size_t i = 0; i < params.size(); i++) {
      payload_ = std::string(params[i].message_size(), 'a');
      std::string test_name =
          base::StringPrintf("Callback_SingleThreadPostTask_Perf_%dx_%zu",
                             params[i].message_count(), payload_.size());
      perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));
      for (int j = 0; j < params[i].message_count(); ++j) {
        ping.Run(payload_,
                 base::Bind(&CallbackPerfTest::SingleThreadPongNoPostTask,
                            base::Unretained(this)));
      }
      perf_logger_.reset();
    }
  }

  void SingleThreadPingNoPostTask(
      const std::string& value,
      const base::Callback<void(const std::string&)>& pong) {
    pong.Run(value);
  }

  void SingleThreadPongNoPostTask(const std::string& value) {}

  void RunSingleThreadPostTaskPingPongServer() {
    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    for (size_t i = 0; i < params.size(); i++) {
      std::string hello("hello");
      base::MessageLoop::current()->task_runner()->PostTask(
          FROM_HERE, base::Bind(&CallbackPerfTest::SingleThreadPingPostTask,
                                base::Unretained(this), hello));
      message_count_ = count_down_ = params[i].message_count();
      payload_ = std::string(params[i].message_size(), 'a');

      base::RunLoop().Run();
    }
  }

  void SingleThreadPingPostTask(const std::string& value) {
    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&CallbackPerfTest::SingleThreadPongPostTask,
                              base::Unretained(this), value));
  }

  void SingleThreadPongPostTask(const std::string& value) {
    if (value == "hello") {
      DCHECK(!perf_logger_.get());
      std::string test_name =
          base::StringPrintf("Callback_SingleThreadNoPostTask_Perf_%dx_%zu",
                             message_count_, payload_.size());
      perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));
    } else {
      DCHECK_EQ(payload_.size(), value.size());

      CHECK(count_down_ > 0);
      count_down_--;
      if (count_down_ == 0) {
        perf_logger_.reset();
        base::MessageLoop::current()->QuitWhenIdle();
        return;
      }
    }

    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&CallbackPerfTest::SingleThreadPingPostTask,
                              base::Unretained(this), payload_));
  }

 private:
  base::Thread client_thread_;
  base::MessageLoop main_message_loop_;
  int message_count_;
  int count_down_;
  std::string payload_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;

  DISALLOW_COPY_AND_ASSIGN(CallbackPerfTest);
};

// Sends the same data as above using PostTask to a different thread instead of
// IPCs for comparison.
TEST_F(CallbackPerfTest, MultiThreadPingPong) {
  RunMultiThreadPingPongServer();
}

// Sends the same data as above using PostTask to the same thread.
TEST_F(CallbackPerfTest, SingleThreadPostTaskPingPong) {
  RunSingleThreadPostTaskPingPongServer();
}

// Sends the same data as above without using PostTask to the same thread.
TEST_F(CallbackPerfTest, SingleThreadNoPostTaskPingPong) {
  RunSingleThreadNoPostTaskPingPongServer();
}

}  // namespace
}  // namespace IPC
