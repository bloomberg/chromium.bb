// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/browser/memory_coordinator.h"

#include "base/memory/memory_pressure_monitor.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace memory_coordinator {

namespace {

class MockMemoryPressureMonitor : public base::MemoryPressureMonitor {
 public:
  ~MockMemoryPressureMonitor() override {}

  void Dispatch(MemoryPressureLevel level) {
    base::MemoryPressureListener::NotifyMemoryPressure(level);
  }

  // MemoryPressureMonitor implementations:
  MemoryPressureLevel GetCurrentPressureLevel() const override {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  void SetDispatchCallback(const DispatchCallback& callback) override {}
};

class MockMemoryCoordinatorClient final : public MemoryCoordinatorClient {
public:
  void OnMemoryStateChange(mojom::MemoryState state) override {
    last_state_ = state;
  }

  mojom::MemoryState last_state() const { return last_state_; }

 private:
  mojom::MemoryState last_state_ = mojom::MemoryState::UNKNOWN;
};

}  // namespace

class MemoryCoordinatorTest : public testing::Test {
 public:
  MemoryCoordinatorTest()
      : message_loop_(new base::MessageLoop),
        coordinator_(new MemoryCoordinator) {}

  MockMemoryPressureMonitor& monitor() { return monitor_; }
  MemoryCoordinator& coordinator() { return *coordinator_.get(); }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  MockMemoryPressureMonitor monitor_;
  std::unique_ptr<MemoryCoordinator> coordinator_;
};

TEST_F(MemoryCoordinatorTest, ReceivePressureLevels) {
  MockMemoryCoordinatorClient client;
  coordinator().RegisterClient(&client);

  {
    base::RunLoop loop;
    monitor().Dispatch(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
    loop.RunUntilIdle();
    EXPECT_EQ(mojom::MemoryState::THROTTLED, client.last_state());
  }

  {
    base::RunLoop loop;
    monitor().Dispatch(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
    loop.RunUntilIdle();
    EXPECT_EQ(mojom::MemoryState::SUSPENDED, client.last_state());
  }
}

}  // namespace memory_coordinator
