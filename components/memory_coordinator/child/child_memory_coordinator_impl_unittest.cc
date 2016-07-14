// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/child/child_memory_coordinator_impl.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_coordinator {

class ChildMemoryCoordinatorImplTest : public testing::Test {
 public:
  ChildMemoryCoordinatorImplTest()
      : message_loop_(new base::MessageLoop),
        coordinator_impl_(nullptr) {
    coordinator_ = coordinator_impl_.binding_.CreateInterfacePtrAndBind();
  }

  void RegisterClient(MemoryCoordinatorClient* client) {
    coordinator_impl_.RegisterClient(client);
  }

  void UnregisterClient(MemoryCoordinatorClient* client) {
    coordinator_impl_.UnregisterClient(client);
  }

  mojom::ChildMemoryCoordinatorPtr& coordinator() { return coordinator_; }
  ChildMemoryCoordinatorImpl& coordinator_impl() { return coordinator_impl_; }

  void ChangeState(mojom::MemoryState state) {
    base::RunLoop loop;
    coordinator()->OnStateChange(state);
    loop.RunUntilIdle();
  }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  ChildMemoryCoordinatorImpl coordinator_impl_;
  mojom::ChildMemoryCoordinatorPtr coordinator_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChildMemoryCoordinatorImplTest);
};

namespace {

class MockMemoryCoordinatorClient final : public MemoryCoordinatorClient {
public:
  void OnMemoryStateChange(mojom::MemoryState state) override {
    last_state_ = state;
  }

  mojom::MemoryState last_state() const { return last_state_; }

 private:
  mojom::MemoryState last_state_ = mojom::MemoryState::UNKNOWN;
};

class MemoryCoordinatorTestThread : public base::Thread,
                                    public MemoryCoordinatorClient {
 public:
  MemoryCoordinatorTestThread(
      const std::string& name,
      ChildMemoryCoordinatorImpl& coordinator)
      : Thread(name), coordinator_(coordinator) {}
  ~MemoryCoordinatorTestThread() override { Stop(); }

  void Init() override {
    coordinator_.RegisterClient(this);
  }

  void OnMemoryStateChange(mojom::MemoryState state) override {
    EXPECT_EQ(message_loop(), base::MessageLoop::current());
    last_state_ = state;
  }

  void CheckLastState(mojom::MemoryState state) {
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryCoordinatorTestThread::CheckLastStateInternal,
                   base::Unretained(this), state));
  }

 private:
  void CheckLastStateInternal(mojom::MemoryState state) {
    base::RunLoop loop;
    loop.RunUntilIdle();
    EXPECT_EQ(state, last_state_);
  }

  ChildMemoryCoordinatorImpl& coordinator_;
  mojom::MemoryState last_state_ = mojom::MemoryState::UNKNOWN;
};

TEST_F(ChildMemoryCoordinatorImplTest, SingleClient) {
  MockMemoryCoordinatorClient client;
  RegisterClient(&client);

  ChangeState(mojom::MemoryState::THROTTLED);
  EXPECT_EQ(mojom::MemoryState::THROTTLED, client.last_state());

  ChangeState(mojom::MemoryState::NORMAL);
  EXPECT_EQ(mojom::MemoryState::NORMAL, client.last_state());

  UnregisterClient(&client);
  ChangeState(mojom::MemoryState::THROTTLED);
  EXPECT_TRUE(mojom::MemoryState::THROTTLED != client.last_state());
}

TEST_F(ChildMemoryCoordinatorImplTest, MultipleClients) {
  MemoryCoordinatorTestThread t1("thread 1", coordinator_impl());
  MemoryCoordinatorTestThread t2("thread 2", coordinator_impl());

  t1.StartAndWaitForTesting();
  t2.StartAndWaitForTesting();

  ChangeState(mojom::MemoryState::THROTTLED);
  t1.CheckLastState(mojom::MemoryState::THROTTLED);
  t2.CheckLastState(mojom::MemoryState::THROTTLED);

  ChangeState(mojom::MemoryState::NORMAL);
  t1.CheckLastState(mojom::MemoryState::NORMAL);
  t2.CheckLastState(mojom::MemoryState::NORMAL);

  t1.Stop();
  t2.Stop();
}

}  // namespace

}  // namespace memory_coordinator
