// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/test/fake_arc_bridge_bootstrap.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class DummyObserver : public ArcBridgeService::Observer {};

}  // namespace

// TODO(hidehiko): ArcBridgeTest gets complicated and has stale code.
// Simplify the code.
class ArcBridgeTest : public testing::Test,
                      public ArcBridgeService::Observer {
 public:
  ArcBridgeTest() = default;

  void OnBridgeReady() override {
    state_ = ArcBridgeService::State::READY;
    ready_ = true;
  }

  void OnBridgeStopped(ArcBridgeService::StopReason stop_reason) override {
    // The instance is already destructed in ArcBridgeServiceImpl::OnStopped().
    state_ = ArcBridgeService::State::STOPPED;
    stop_reason_ = stop_reason;
    message_loop_.task_runner()->PostTask(FROM_HERE,
                                          message_loop_.QuitWhenIdleClosure());
  }

  bool ready() const { return ready_; }
  ArcBridgeService::State state() const { return state_; }
  FakeArcBridgeBootstrap* bootstrap() const {
    return static_cast<FakeArcBridgeBootstrap*>(
        service_->GetBootstrapForTesting());
  }

 protected:
  std::unique_ptr<ArcBridgeServiceImpl> service_;
  ArcBridgeService::StopReason stop_reason_;

  static std::unique_ptr<ArcBridgeBootstrap> CreateSuspendedBootstrap() {
    auto bootstrap = base::MakeUnique<FakeArcBridgeBootstrap>();
    bootstrap->SuspendBoot();
    return std::move(bootstrap);
  }

  static std::unique_ptr<ArcBridgeBootstrap> CreateBootFailureBootstrap(
      ArcBridgeService::StopReason reason) {
    auto bootstrap = base::MakeUnique<FakeArcBridgeBootstrap>();
    bootstrap->EnableBootFailureEmulation(reason);
    return std::move(bootstrap);
  }

 private:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    ready_ = false;
    state_ = ArcBridgeService::State::STOPPED;
    stop_reason_ = ArcBridgeService::StopReason::SHUTDOWN;

    service_.reset(new ArcBridgeServiceImpl());
    service_->SetArcBridgeBootstrapFactoryForTesting(
        base::Bind(FakeArcBridgeBootstrap::Create));
    service_->AddObserver(this);
  }

  void TearDown() override {
    service_->RemoveObserver(this);
    service_.reset();

    chromeos::DBusThreadManager::Shutdown();
  }

  bool ready_ = false;
  ArcBridgeService::State state_;
  base::MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeTest);
};

// Exercises the basic functionality of the ARC Bridge Service.  A message from
// within the instance should cause the observer to be notified.
TEST_F(ArcBridgeTest, Basic) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());

  service_->HandleStartup();
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If the ArcBridgeService is shut down, it should be stopped, even
// mid-startup.
TEST_F(ArcBridgeTest, ShutdownMidStartup) {
  ASSERT_FALSE(ready());

  service_->SetArcBridgeBootstrapFactoryForTesting(
      base::Bind(ArcBridgeTest::CreateSuspendedBootstrap));
  service_->HandleStartup();
  ASSERT_FALSE(service_->stopped());
  ASSERT_FALSE(service_->ready());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If the boot procedure is failed, then restarting mechanism should not
// triggered.
TEST_F(ArcBridgeTest, BootFailure) {
  ASSERT_TRUE(service_->stopped());

  service_->SetArcBridgeBootstrapFactoryForTesting(
      base::Bind(ArcBridgeTest::CreateBootFailureBootstrap,
                 ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE));
  service_->HandleStartup();
  EXPECT_EQ(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE, stop_reason_);
  ASSERT_TRUE(service_->stopped());
}

// If the instance is stopped, it should be re-started.
TEST_F(ArcBridgeTest, Restart) {
  ASSERT_FALSE(ready());

  service_->HandleStartup();
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  // Simulate a connection loss.
  service_->DisableReconnectDelayForTesting();
  ASSERT_TRUE(bootstrap());
  bootstrap()->StopWithReason(ArcBridgeService::StopReason::CRASH);
  ASSERT_TRUE(service_->ready());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// Makes sure OnBridgeStopped is called on stop.
TEST_F(ArcBridgeTest, OnBridgeStopped) {
  ASSERT_FALSE(ready());

  service_->DisableReconnectDelayForTesting();
  service_->HandleStartup();
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  // Simulate boot failure.
  ASSERT_TRUE(bootstrap());
  bootstrap()->StopWithReason(
      ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE);
  EXPECT_EQ(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE, stop_reason_);
  ASSERT_TRUE(service_->ready());

  // Simulate crash.
  ASSERT_TRUE(bootstrap());
  bootstrap()->StopWithReason(ArcBridgeService::StopReason::CRASH);
  EXPECT_EQ(ArcBridgeService::StopReason::CRASH, stop_reason_);
  ASSERT_TRUE(service_->ready());

  // Graceful shutdown.
  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::StopReason::SHUTDOWN, stop_reason_);
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// Removing the same observer more than once should be okay.
TEST_F(ArcBridgeTest, RemoveObserverTwice) {
  ASSERT_FALSE(ready());
  auto dummy_observer = base::MakeUnique<DummyObserver>();
  service_->AddObserver(dummy_observer.get());
  // Call RemoveObserver() twice.
  service_->RemoveObserver(dummy_observer.get());
  service_->RemoveObserver(dummy_observer.get());
}

// Removing an unknown observer should be allowed.
TEST_F(ArcBridgeTest, RemoveUnknownObserver) {
  ASSERT_FALSE(ready());
  auto dummy_observer = base::MakeUnique<DummyObserver>();
  service_->RemoveObserver(dummy_observer.get());
}

}  // namespace arc
