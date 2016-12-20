// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/test/fake_arc_session.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class DummyObserver : public ArcSessionObserver {};

}  // namespace

class ArcBridgeTest : public testing::Test, public ArcSessionObserver {
 public:
  ArcBridgeTest() = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    stop_reason_ = StopReason::SHUTDOWN;

    // We inject FakeArcSession here so we do not need task_runner.
    service_ = base::MakeUnique<ArcBridgeServiceImpl>(nullptr);
    service_->SetArcSessionFactoryForTesting(
        base::Bind(FakeArcSession::Create));
    service_->AddObserver(this);
  }

  void TearDown() override {
    service_->RemoveObserver(this);
    service_.reset();

    chromeos::DBusThreadManager::Shutdown();
  }

  ArcBridgeServiceImpl* arc_bridge_service() { return service_.get(); }

  FakeArcSession* arc_session() const {
    return static_cast<FakeArcSession*>(service_->GetArcSessionForTesting());
  }

  StopReason stop_reason() { return stop_reason_; }

  static std::unique_ptr<ArcSession> CreateSuspendedArcSession() {
    auto arc_session = base::MakeUnique<FakeArcSession>();
    arc_session->SuspendBoot();
    return std::move(arc_session);
  }

  static std::unique_ptr<ArcSession> CreateBootFailureArcSession(
      StopReason reason) {
    auto arc_session = base::MakeUnique<FakeArcSession>();
    arc_session->EnableBootFailureEmulation(reason);
    return std::move(arc_session);
  }

 private:
  // ArcSessionObserver:
  void OnSessionStopped(StopReason stop_reason) override {
    // The instance is already destructed in ArcBridgeServiceImpl::OnStopped().
    stop_reason_ = stop_reason;
  }

  StopReason stop_reason_;
  std::unique_ptr<ArcBridgeServiceImpl> service_;
  base::MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeTest);
};

// Exercises the basic functionality of the ARC Bridge Service. Observer should
// be notified.
TEST_F(ArcBridgeTest, Basic) {
  class Observer : public ArcSessionObserver {
   public:
    Observer() = default;

    bool IsReadyCalled() { return ready_called_; }
    bool IsStoppedCalled() { return stopped_called_; }

    // ArcSessionObserver:
    void OnSessionReady() override { ready_called_ = true; }
    void OnSessionStopped(StopReason stop_reason) override {
      stopped_called_ = true;
    }

   private:
    bool ready_called_ = false;
    bool stopped_called_ = false;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  Observer observer;
  arc_bridge_service()->AddObserver(&observer);
  base::ScopedClosureRunner teardown(base::Bind(
      [](ArcBridgeService* arc_bridge_service, Observer* observer) {
        arc_bridge_service->RemoveObserver(observer);
      },
      arc_bridge_service(), &observer));

  EXPECT_TRUE(arc_bridge_service()->stopped());

  arc_bridge_service()->RequestStart();
  EXPECT_TRUE(arc_bridge_service()->ready());
  EXPECT_TRUE(observer.IsReadyCalled());

  arc_bridge_service()->RequestStop();
  EXPECT_TRUE(arc_bridge_service()->stopped());
  EXPECT_TRUE(observer.IsStoppedCalled());
}

// If the ArcBridgeService accepts a request to stop ARC instance, it should
// stop it, even mid-startup.
TEST_F(ArcBridgeTest, StopMidStartup) {
  arc_bridge_service()->SetArcSessionFactoryForTesting(
      base::Bind(ArcBridgeTest::CreateSuspendedArcSession));
  EXPECT_TRUE(arc_bridge_service()->stopped());

  arc_bridge_service()->RequestStart();
  EXPECT_FALSE(arc_bridge_service()->stopped());
  EXPECT_FALSE(arc_bridge_service()->ready());

  arc_bridge_service()->RequestStop();
  EXPECT_TRUE(arc_bridge_service()->stopped());
}

// If the boot procedure is failed, then restarting mechanism should not
// triggered.
TEST_F(ArcBridgeTest, BootFailure) {
  arc_bridge_service()->SetArcSessionFactoryForTesting(
      base::Bind(ArcBridgeTest::CreateBootFailureArcSession,
                 StopReason::GENERIC_BOOT_FAILURE));
  EXPECT_TRUE(arc_bridge_service()->stopped());

  arc_bridge_service()->RequestStart();
  EXPECT_EQ(StopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_TRUE(arc_bridge_service()->stopped());
}

// If the instance is stopped, it should be re-started.
TEST_F(ArcBridgeTest, Restart) {
  arc_bridge_service()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_bridge_service()->stopped());

  arc_bridge_service()->RequestStart();
  EXPECT_TRUE(arc_bridge_service()->ready());

  // Simulate a connection loss.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(StopReason::CRASH);
  EXPECT_TRUE(arc_bridge_service()->stopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_bridge_service()->ready());

  arc_bridge_service()->RequestStop();
  EXPECT_TRUE(arc_bridge_service()->stopped());
}

// Makes sure OnSessionStopped is called on stop.
TEST_F(ArcBridgeTest, OnSessionStopped) {
  arc_bridge_service()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_bridge_service()->stopped());

  arc_bridge_service()->RequestStart();
  EXPECT_TRUE(arc_bridge_service()->ready());

  // Simulate boot failure.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(StopReason::GENERIC_BOOT_FAILURE);
  EXPECT_EQ(StopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_TRUE(arc_bridge_service()->stopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_bridge_service()->ready());

  // Simulate crash.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(StopReason::CRASH);
  EXPECT_EQ(StopReason::CRASH, stop_reason());
  EXPECT_TRUE(arc_bridge_service()->stopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_bridge_service()->ready());

  // Graceful stop.
  arc_bridge_service()->RequestStop();
  EXPECT_EQ(StopReason::SHUTDOWN, stop_reason());
  EXPECT_TRUE(arc_bridge_service()->stopped());
}

TEST_F(ArcBridgeTest, Shutdown) {
  arc_bridge_service()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_bridge_service()->stopped());

  arc_bridge_service()->RequestStart();
  EXPECT_TRUE(arc_bridge_service()->ready());

  // Simulate shutdown.
  arc_bridge_service()->OnShutdown();
  EXPECT_EQ(StopReason::SHUTDOWN, stop_reason());
  EXPECT_TRUE(arc_bridge_service()->stopped());
}

// Removing the same observer more than once should be okay.
TEST_F(ArcBridgeTest, RemoveObserverTwice) {
  EXPECT_TRUE(arc_bridge_service()->stopped());

  DummyObserver dummy_observer;
  arc_bridge_service()->AddObserver(&dummy_observer);
  // Call RemoveObserver() twice.
  arc_bridge_service()->RemoveObserver(&dummy_observer);
  arc_bridge_service()->RemoveObserver(&dummy_observer);
}

// Removing an unknown observer should be allowed.
TEST_F(ArcBridgeTest, RemoveUnknownObserver) {
  EXPECT_TRUE(arc_bridge_service()->stopped());

  DummyObserver dummy_observer;
  arc_bridge_service()->RemoveObserver(&dummy_observer);
}

}  // namespace arc
