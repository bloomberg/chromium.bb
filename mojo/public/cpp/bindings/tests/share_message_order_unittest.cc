// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/share_message_order.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace share_message_order {

using ShareMessageOrderTest = mojo::core::test::MojoTestBase;

class CounterObserverImpl : public mojom::CounterObserver {
 public:
  CounterObserverImpl() = default;
  ~CounterObserverImpl() override = default;

  uint32_t counter_value() const { return counter_value_; }

  PendingRemote<mojom::CounterObserver> MakeRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitForNextIncrement() {
    if (!wait_loop_)
      wait_loop_.emplace();
    wait_loop_->Run();
    wait_loop_.reset();
  }

  // mojom::CounterObserver:
  void OnIncrement(uint32_t value) override {
    counter_value_ = value;
    if (wait_loop_)
      wait_loop_->Quit();
  }

 private:
  uint32_t counter_value_ = 0;
  Receiver<mojom::CounterObserver> receiver_{this};
  base::Optional<base::RunLoop> wait_loop_;

  DISALLOW_COPY_AND_ASSIGN(CounterObserverImpl);
};

class CounterImpl : public mojom::Counter, public mojom::Doubler {
 public:
  explicit CounterImpl(PendingReceiver<mojom::Counter> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(wait_for_disconnect_loop_.QuitClosure());
  }

  ~CounterImpl() override = default;

  void WaitForDisconnect() { wait_for_disconnect_loop_.Run(); }

 private:
  // mojom::Counter:
  void Increment(IncrementCallback callback) override {
    ++value_;
    std::move(callback).Run();
    for (const auto& observer : observers_)
      observer->OnIncrement(value_);
  }

  void AddObserver(PendingRemote<mojom::CounterObserver> observer) override {
    observers_.emplace_back(std::move(observer));
  }

  void AddDoubler(PendingReceiver<mojom::Doubler> receiver) override {
    doubler_receiver_.Bind(std::move(receiver));
  }

  // mojom::Doubler:
  void Double() override { value_ *= 2; }

  base::RunLoop wait_for_disconnect_loop_;
  uint32_t value_ = 0;
  Receiver<mojom::Counter> receiver_;
  Receiver<mojom::Doubler> doubler_receiver_{this};
  std::vector<Remote<mojom::CounterObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(CounterImpl);
};

TEST_F(ShareMessageOrderTest, Ordering) {
  // Setup two child processes, one for a CounterImpl and one for its client.
  // They will use additional interfaces with shared message ordering. We use
  // a multi-process test environment because it introduces sufficient internal
  // timing variations to exercise our ordering constraints.
  RunTestClient("CounterImpl", [&](MojoHandle h) {
    MojoHandle receiver_handle, remote_handle;
    CreateMessagePipe(&receiver_handle, &remote_handle);

    WriteMessageWithHandles(h, "hi", &receiver_handle, 1);
    RunTestClient("CounterClient", [&](MojoHandle h) {
      WriteMessageWithHandles(h, "hi", &remote_handle, 1);
      EXPECT_EQ("ok", ReadMessage(h));
      WriteMessage(h, "bye");
    });
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CounterImpl, ShareMessageOrderTest, h) {
  base::test::ScopedTaskEnvironment task_environment;

  MojoHandle receiver_handle;
  EXPECT_EQ("hi", ReadMessageWithHandles(h, &receiver_handle, 1));

  CounterImpl counter_impl{PendingReceiver<mojom::Counter>(
      ScopedMessagePipeHandle(MessagePipeHandle(receiver_handle)))};
  counter_impl.WaitForDisconnect();
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CounterClient, ShareMessageOrderTest, h) {
  base::test::ScopedTaskEnvironment task_environment;

  MojoHandle remote_handle;
  EXPECT_EQ("hi", ReadMessageWithHandles(h, &remote_handle, 1));

  Remote<mojom::Counter> counter{PendingRemote<mojom::Counter>(
      ScopedMessagePipeHandle(MessagePipeHandle(remote_handle)), 0)};

  // By the mojom definition of AddDoubler, the Doubler's own pipe will share
  // message ordering with the Counter's pipe once this message is sent.
  Remote<mojom::Doubler> doubler;
  counter->AddDoubler(doubler.BindNewPipeAndPassReceiver());

  // And the CounterObserver's pipe will share messaging ordering as well,
  // specifically its received messages will be ordered with replies received
  // by our Remote<mojom::Counter>.
  CounterObserverImpl observer;
  counter->AddObserver(observer.MakeRemote());

  {
    base::RunLoop loop;
    counter->Increment(loop.QuitClosure());
    loop.Run();
  }

  // The observer should not have dispatched an observed event yet, since it
  // must arrive after the reply which just terminated our RunLoop.
  //
  // If there are ordering violations, this may flakily fail with an unexpected
  // value of 1.
  EXPECT_EQ(0u, observer.counter_value());
  observer.WaitForNextIncrement();

  EXPECT_EQ(1u, observer.counter_value());

  // Also verify ordering of messages on the Doubler.

  {
    base::RunLoop loop;
    counter->Increment(base::DoNothing());   // Increment to 2
    doubler->Double();                       // Double to 4
    counter->Increment(loop.QuitClosure());  // Increment to 5
    loop.Run();
  }

  // Because of strict message ordering constraints, at this point the observer
  // should have seen the increment to 2 above, but not the increment to 5.
  //
  // If there are ordering violations, this may flakily fail with an unexpected
  // value of 2, 3, or 4.
  EXPECT_EQ(2u, observer.counter_value());

  observer.WaitForNextIncrement();

  // If there are ordering violations, this may flakily fail with an unexpected
  // value of 3 or 4.
  EXPECT_EQ(5u, observer.counter_value());

  WriteMessage(h, "ok");
  EXPECT_EQ("bye", ReadMessage(h));
}

class SyncPingImpl : public mojom::SyncPing {
 public:
  SyncPingImpl(mojo::PendingReceiver<mojom::SyncPing> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~SyncPingImpl() override = default;

  // mojom::SyncPing:
  void PingAsync(PingCallback callback) override { std::move(callback).Run(); }
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

 private:
  mojo::Receiver<mojom::SyncPing> receiver_;

  DISALLOW_COPY_AND_ASSIGN(SyncPingImpl);
};

class SyncEchoImpl : public mojom::SyncEcho {
 public:
  SyncEchoImpl(mojo::PendingReceiver<mojom::SyncEcho> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~SyncEchoImpl() override = default;

  // mojom::SyncEcho:
  void PingThenEcho(mojo::PendingRemote<mojom::SyncPing> remote_ping,
                    const std::string& x,
                    PingThenEchoCallback callback) override {
    mojo::Remote<mojom::SyncPing> ping(std::move(remote_ping));

    base::RunLoop loop;
    ping->PingAsync(loop.QuitClosure());
    loop.Run();

    CHECK(ping->Ping());

    std::move(callback).Run(x);
  }

 private:
  mojo::Receiver<mojom::SyncEcho> receiver_;

  DISALLOW_COPY_AND_ASSIGN(SyncEchoImpl);
};

TEST_F(ShareMessageOrderTest, NestedSyncCall) {
  base::test::ScopedTaskEnvironment task_environment;

  mojo::PendingRemote<mojom::SyncPing> ping;
  SyncPingImpl ping_impl(ping.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::SyncEcho> echo;
  SyncEchoImpl echo_impl(echo.BindNewPipeAndPassReceiver());

  const std::string kTestString = "ok hello";
  std::string echoed_string;
  EXPECT_TRUE(echo->PingThenEcho(std::move(ping), kTestString, &echoed_string));
  EXPECT_EQ(kTestString, echoed_string);
}

}  // namespace share_message_order
}  // namespace test
}  // namespace mojo
