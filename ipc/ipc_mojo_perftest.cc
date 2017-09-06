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
#include "base/synchronization/waitable_event.h"
#include "base/test/perf_time_logger.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_perftest_messages.h"
#include "ipc/ipc_perftest_util.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_test.mojom.h"
#include "ipc/ipc_test_base.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {
namespace {

class PerformanceChannelListener : public Listener {
 public:
  explicit PerformanceChannelListener(const std::string& label)
      : label_(label),
        sender_(NULL),
        msg_count_(0),
        msg_size_(0),
        sync_(false),
        count_down_(0) {
    VLOG(1) << "Server listener up";
  }

  ~PerformanceChannelListener() override { VLOG(1) << "Server listener down"; }

  void Init(Sender* sender) {
    DCHECK(!sender_);
    sender_ = sender;
  }

  // Call this before running the message loop.
  void SetTestParams(int msg_count, size_t msg_size, bool sync) {
    DCHECK_EQ(0, count_down_);
    msg_count_ = msg_count;
    msg_size_ = msg_size;
    sync_ = sync;
    count_down_ = msg_count_;
    payload_ = std::string(msg_size_, 'a');
  }

  bool OnMessageReceived(const Message& message) override {
    CHECK(sender_);

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PerformanceChannelListener, message)
      IPC_MESSAGE_HANDLER(TestMsg_Hello, OnHello)
      IPC_MESSAGE_HANDLER(TestMsg_Ping, OnPing)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnHello() {
    // Start timing on hello.
    DCHECK(!perf_logger_.get());
    std::string test_name =
        base::StringPrintf("IPC_%s_Perf_%dx_%u", label_.c_str(), msg_count_,
                           static_cast<unsigned>(msg_size_));
    perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));
    if (sync_) {
      for (; count_down_ > 0; --count_down_) {
        std::string response;
        sender_->Send(new TestMsg_SyncPing(payload_, &response));
        DCHECK_EQ(response, payload_);
      }
      perf_logger_.reset();
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
    } else {
      SendPong();
    }
  }

  void OnPing(const std::string& payload) {
    // Include message deserialization in latency.
    DCHECK_EQ(payload_.size(), payload.size());

    CHECK(count_down_ > 0);
    count_down_--;
    if (count_down_ == 0) {
      perf_logger_.reset();  // Stop the perf timer now.
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
      return;
    }

    SendPong();
  }

  void SendPong() { sender_->Send(new TestMsg_Ping(payload_)); }

 private:
  std::string label_;
  Sender* sender_;
  int msg_count_;
  size_t msg_size_;
  bool sync_;

  int count_down_;
  std::string payload_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;
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

class InterfacePassingTestParams {
 public:
  InterfacePassingTestParams(size_t rounds, size_t num_interfaces)
      : rounds_(rounds), num_interfaces_(num_interfaces) {}

  size_t rounds() const { return rounds_; }
  size_t num_interfaces() const { return num_interfaces_; }

 private:
  size_t rounds_;
  size_t num_interfaces_;
};

#ifdef NDEBUG
const int kMultiplier = 100;
#else
  // Debug builds on Windows run these tests orders of magnitude more slowly.
const int kMultiplier = 1;
#endif

std::vector<PingPongTestParams> GetDefaultTestParams() {
  // Test several sizes. We use 12^N for message size, and limit the message
  // count to keep the test duration reasonable.
  std::vector<PingPongTestParams> list;
  list.push_back(PingPongTestParams(12, 500 * kMultiplier));
  list.push_back(PingPongTestParams(144, 500 * kMultiplier));
  list.push_back(PingPongTestParams(1728, 500 * kMultiplier));
  list.push_back(PingPongTestParams(20736, 120 * kMultiplier));
  list.push_back(PingPongTestParams(248832, 10 * kMultiplier));
  return list;
}

std::vector<InterfacePassingTestParams> GetDefaultInterfacePassingTestParams() {
  std::vector<InterfacePassingTestParams> list;
  list.push_back({500 * kMultiplier, 0});
  list.push_back({500 * kMultiplier, 1});
  list.push_back({500 * kMultiplier, 2});
  list.push_back({500 * kMultiplier, 4});
  list.push_back({500 * kMultiplier, 8});
  return list;
}

class MojoChannelPerfTest : public IPCChannelMojoTestBase {
 public:
  MojoChannelPerfTest() = default;
  ~MojoChannelPerfTest() override = default;

  void RunTestChannelProxyPingPong() {
    Init("MojoPerfTestClient");

    // Set up IPC channel and start client.
    PerformanceChannelListener listener("ChannelProxy");
    auto channel_proxy = IPC::ChannelProxy::Create(
        TakeHandle().release(), IPC::Channel::MODE_SERVER, &listener,
        GetIOThreadTaskRunner());
    listener.Init(channel_proxy.get());

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    for (size_t i = 0; i < params.size(); i++) {
      listener.SetTestParams(params[i].message_count(),
                             params[i].message_size(), false);

      // This initial message will kick-start the ping-pong of messages.
      channel_proxy->Send(new TestMsg_Hello);

      // Run message loop.
      base::RunLoop().Run();
    }

    // Send quit message.
    channel_proxy->Send(new TestMsg_Quit);

    EXPECT_TRUE(WaitForClientShutdown());
    channel_proxy.reset();
  }

  void RunTestChannelProxySyncPing() {
    Init("MojoPerfTestClient");

    // Set up IPC channel and start client.
    PerformanceChannelListener listener("ChannelProxy");
    base::WaitableEvent shutdown_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    auto channel_proxy = IPC::SyncChannel::Create(
        TakeHandle().release(), IPC::Channel::MODE_SERVER, &listener,
        GetIOThreadTaskRunner(), false, &shutdown_event);
    listener.Init(channel_proxy.get());

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<PingPongTestParams> params = GetDefaultTestParams();
    for (size_t i = 0; i < params.size(); i++) {
      listener.SetTestParams(params[i].message_count(),
                             params[i].message_size(), true);

      // This initial message will kick-start the ping-pong of messages.
      channel_proxy->Send(new TestMsg_Hello);

      // Run message loop.
      base::RunLoop().Run();
    }

    // Send quit message.
    channel_proxy->Send(new TestMsg_Quit);

    EXPECT_TRUE(WaitForClientShutdown());
    channel_proxy.reset();
  }
};

TEST_F(MojoChannelPerfTest, ChannelProxyPingPong) {
  RunTestChannelProxyPingPong();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(MojoChannelPerfTest, ChannelProxySyncPing) {
  RunTestChannelProxySyncPing();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

MULTIPROCESS_TEST_MAIN(MojoPerfTestClientTestChildMain) {
  MojoPerfTestClient client;
  int rv = mojo::edk::test::MultiprocessTestHelper::RunClientMain(
      base::Bind(&MojoPerfTestClient::Run, base::Unretained(&client)),
      true /* pass_pipe_ownership_to_main */);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return rv;
}

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
        base::RunLoop::QuitCurrentWhenIdleDeprecated();
        return;
      }
    }

    if (sync_) {
      for (int i = 0; i < count_down_; ++i) {
        std::string response;
        ping_receiver_->SyncPing(payload_, &response);
        DCHECK_EQ(response, payload_);
      }
      perf_logger_.reset();
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
    } else {
      ping_receiver_->Ping(payload_, base::Bind(&MojoInterfacePerfTest::OnPong,
                                                base::Unretained(this)));
    }
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
    base::RunLoop run_loop;
    ReflectorImpl impl(std::move(scoped_mp), run_loop.QuitWhenIdleClosure());
    run_loop.Run();
    return 0;
  }

  bool sync_ = false;

 private:
  int message_count_;
  int count_down_;
  std::string label_;
  std::string payload_;
  IPC::mojom::ReflectorPtr ping_receiver_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;

  DISALLOW_COPY_AND_ASSIGN(MojoInterfacePerfTest);
};

class InterfacePassingTestDriverImpl : public mojom::InterfacePassingTestDriver,
                                       public mojom::PingReceiver {
 public:
  InterfacePassingTestDriverImpl(mojo::ScopedMessagePipeHandle handle,
                                 const base::Closure& quit_closure)
      : binding_(this,
                 mojom::InterfacePassingTestDriverRequest(std::move(handle))),
        quit_closure_(quit_closure) {}
  ~InterfacePassingTestDriverImpl() override {
    ignore_result(binding_.Unbind().PassMessagePipe().release());
  }

 private:
  // mojom::InterfacePassingTestDriver implementation:
  void Init(InitCallback callback) override { std::move(callback).Run(); }

  void GetPingReceiver(std::vector<mojom::PingReceiverRequest> requests,
                       GetPingReceiverCallback callback) override {
    for (auto& request : requests)
      ping_receiver_bindings_.AddBinding(this, std::move(request));
    ping_receiver_bindings_.CloseAllBindings();
    std::move(callback).Run();
  }

  void GetAssociatedPingReceiver(
      std::vector<mojom::PingReceiverAssociatedRequest> requests,
      GetAssociatedPingReceiverCallback callback) override {
    for (auto& request : requests)
      ping_receiver_associated_bindings_.AddBinding(this, std::move(request));
    ping_receiver_associated_bindings_.CloseAllBindings();
    std::move(callback).Run();
  }

  void Quit() override {
    if (quit_closure_)
      quit_closure_.Run();
  }

  // mojom::PingReceiver implementation:
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  mojo::BindingSet<mojom::PingReceiver> ping_receiver_bindings_;
  mojo::AssociatedBindingSet<mojom::PingReceiver>
      ping_receiver_associated_bindings_;
  mojo::Binding<mojom::InterfacePassingTestDriver> binding_;

  base::Closure quit_closure_;
};

class MojoInterfacePassingPerfTest : public mojo::edk::test::MojoTestBase {
 public:
  MojoInterfacePassingPerfTest() = default;

 protected:
  void RunInterfacePassingServer(MojoHandle mp,
                                 const std::string& label,
                                 bool associated) {
    label_ = label;
    associated_ = associated;

    mojo::MessagePipeHandle mp_handle(mp);
    mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);
    driver_ptr_.Bind(
        mojom::InterfacePassingTestDriverPtrInfo(std::move(scoped_mp), 0u));

    auto params = GetDefaultInterfacePassingTestParams();

    LockThreadAffinity thread_locker(kSharedCore);
    for (size_t i = 0; i < params.size(); ++i) {
      driver_ptr_->Init(
          base::Bind(&MojoInterfacePassingPerfTest::OnInitCallback,
                     base::Unretained(this)));
      rounds_ = count_down_ = params[i].rounds();
      num_interfaces_ = params[i].num_interfaces();

      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitWhenIdleClosure();
      run_loop.Run();
    }

    driver_ptr_->Quit();

    ignore_result(driver_ptr_.PassInterface().PassHandle().release());
  }

  void OnInitCallback() {
    DCHECK(!perf_logger_.get());
    std::string test_name = base::StringPrintf(
        "IPC_%s_Perf_%zux_%zu", label_.c_str(), rounds_, num_interfaces_);
    perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));

    DoNextRound();
  }

  void DoNextRound() {
    if (associated_) {
      std::vector<mojom::PingReceiverAssociatedPtr> associated_interfaces(
          num_interfaces_);

      std::vector<mojom::PingReceiverAssociatedRequest> requests(
          num_interfaces_);
      for (size_t i = 0; i < num_interfaces_; ++i) {
        requests[i] = mojo::MakeRequest(&associated_interfaces[i]);
        // Force the interface pointer to do full initialization.
        associated_interfaces[i].get();
      }

      driver_ptr_->GetAssociatedPingReceiver(
          std::move(requests),
          base::Bind(&MojoInterfacePassingPerfTest::OnGetReceiverCallback,
                     base::Unretained(this)));
    } else {
      std::vector<mojom::PingReceiverPtr> interfaces(num_interfaces_);

      std::vector<mojom::PingReceiverRequest> requests(num_interfaces_);
      for (size_t i = 0; i < num_interfaces_; ++i) {
        requests[i] = mojo::MakeRequest(&interfaces[i]);
        // Force the interface pointer to do full initialization.
        interfaces[i].get();
      }

      driver_ptr_->GetPingReceiver(
          std::move(requests),
          base::Bind(&MojoInterfacePassingPerfTest::OnGetReceiverCallback,
                     base::Unretained(this)));
    }
  }

  void OnGetReceiverCallback() {
    CHECK_GT(count_down_, 0u);
    count_down_--;

    if (count_down_ == 0) {
      perf_logger_.reset();
      quit_closure_.Run();
      return;
    }

    DoNextRound();
  }

  static int RunInterfacePassingClient(MojoHandle mp) {
    mojo::MessagePipeHandle mp_handle(mp);
    mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);

    // In single process mode, this is running in a task and by default other
    // tasks (in particular, the binding) won't run. To keep the single process
    // and multi-process code paths the same, enable nestable tasks.
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());

    LockThreadAffinity thread_locker(kSharedCore);
    base::RunLoop run_loop;
    InterfacePassingTestDriverImpl impl(std::move(scoped_mp),
                                        run_loop.QuitWhenIdleClosure());
    run_loop.Run();
    return 0;
  }

 private:
  size_t rounds_ = 0;
  size_t count_down_ = 0;
  size_t num_interfaces_ = 0;
  std::string label_;
  bool associated_ = false;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;

  mojom::InterfacePassingTestDriverPtr driver_ptr_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(MojoInterfacePassingPerfTest);
};

DEFINE_TEST_CLIENT_WITH_PIPE(InterfacePassingClient,
                             MojoInterfacePassingPerfTest,
                             h) {
  base::MessageLoop main_message_loop;
  return RunInterfacePassingClient(h);
}

enum class InProcessMessageMode {
  kSerialized,
  kUnserialized,
};

template <class TestBase>
class InProcessPerfTest
    : public TestBase,
      public testing::WithParamInterface<InProcessMessageMode> {
 public:
  InProcessPerfTest() {
    switch (GetParam()) {
      case InProcessMessageMode::kSerialized:
        mojo::Connector::OverrideDefaultSerializationBehaviorForTesting(
            mojo::Connector::OutgoingSerializationMode::kEager,
            mojo::Connector::IncomingSerializationMode::kDispatchAsIs);
        break;
      case InProcessMessageMode::kUnserialized:
        mojo::Connector::OverrideDefaultSerializationBehaviorForTesting(
            mojo::Connector::OutgoingSerializationMode::kLazy,
            mojo::Connector::IncomingSerializationMode::kDispatchAsIs);
        break;
    }
  }
};

using MojoInProcessInterfacePerfTest = InProcessPerfTest<MojoInterfacePerfTest>;
using MojoInProcessInterfacePassingPerfTest =
    InProcessPerfTest<MojoInterfacePassingPerfTest>;

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

TEST_F(MojoInterfacePerfTest, MultiprocessSyncPing) {
  sync_ = true;
  RunTestClient("PingPongClient", [&](MojoHandle h) {
    base::MessageLoop main_message_loop;
    RunPingPongServer(h, "MultiprocessSync");
  });
}

TEST_F(MojoInterfacePassingPerfTest, MultiprocessInterfacePassing) {
  RunTestClient("InterfacePassingClient", [&](MojoHandle h) {
    base::MessageLoop main_message_loop;
    RunInterfacePassingServer(h, "InterfacePassing", false /* associated */);
  });
}

TEST_F(MojoInterfacePassingPerfTest, MultiprocessAssociatedInterfacePassing) {
  RunTestClient("InterfacePassingClient", [&](MojoHandle h) {
    base::MessageLoop main_message_loop;
    RunInterfacePassingServer(h, "AssociatedInterfacePassing",
                              true /* associated*/);
  });
}

// A single process version of the above test.
TEST_P(MojoInProcessInterfacePerfTest, MultiThreadPingPong) {
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

TEST_P(MojoInProcessInterfacePerfTest, SingleThreadPingPong) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::MessageLoop main_message_loop;
  mojo::MessagePipeHandle mp_handle(client_handle);
  mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);
  LockThreadAffinity thread_locker(kSharedCore);
  ReflectorImpl impl(std::move(scoped_mp), base::Closure());

  RunPingPongServer(server_handle, "SingleProcess");
}

INSTANTIATE_TEST_CASE_P(,
                        MojoInProcessInterfacePerfTest,
                        testing::Values(InProcessMessageMode::kSerialized,
                                        InProcessMessageMode::kUnserialized));

TEST_P(MojoInProcessInterfacePassingPerfTest, MultiThreadInterfacePassing) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::Thread client_thread("InterfacePassingClient");
  client_thread.Start();
  client_thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(base::IgnoreResult(&RunInterfacePassingClient),
                            client_handle));

  base::MessageLoop main_message_loop;
  RunInterfacePassingServer(server_handle, "SingleProcess",
                            false /* associated */);
}

TEST_P(MojoInProcessInterfacePassingPerfTest,
       MultiThreadAssociatedInterfacePassing) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::Thread client_thread("InterfacePassingClient");
  client_thread.Start();
  client_thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(base::IgnoreResult(&RunInterfacePassingClient),
                            client_handle));

  base::MessageLoop main_message_loop;
  RunInterfacePassingServer(server_handle, "SingleProcess",
                            true /* associated */);
}

TEST_P(MojoInProcessInterfacePassingPerfTest, SingleThreadInterfacePassing) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::MessageLoop main_message_loop;
  mojo::MessagePipeHandle mp_handle(client_handle);
  mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);
  LockThreadAffinity thread_locker(kSharedCore);
  InterfacePassingTestDriverImpl impl(std::move(scoped_mp), base::Closure());

  RunInterfacePassingServer(server_handle, "SingleProcess",
                            false /* associated */);
}

TEST_P(MojoInProcessInterfacePassingPerfTest,
       SingleThreadAssociatedInterfacePassing) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::MessageLoop main_message_loop;
  mojo::MessagePipeHandle mp_handle(client_handle);
  mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);
  LockThreadAffinity thread_locker(kSharedCore);
  InterfacePassingTestDriverImpl impl(std::move(scoped_mp), base::Closure());

  RunInterfacePassingServer(server_handle, "SingleProcess",
                            true /* associated */);
}

INSTANTIATE_TEST_CASE_P(,
                        MojoInProcessInterfacePassingPerfTest,
                        testing::Values(InProcessMessageMode::kSerialized,
                                        InProcessMessageMode::kUnserialized));

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
        base::RunLoop::QuitCurrentWhenIdleDeprecated();
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
        base::RunLoop::QuitCurrentWhenIdleDeprecated();
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
