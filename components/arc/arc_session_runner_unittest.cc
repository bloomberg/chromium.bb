// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class DummyObserver : public ArcSessionObserver {};

}  // namespace

class ArcSessionRunnerTest : public testing::Test, public ArcSessionObserver {
 public:
  ArcSessionRunnerTest() = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    stop_reason_ = StopReason::SHUTDOWN;

    // We inject FakeArcSession here so we do not need task_runner.
    arc_session_runner_ =
        base::MakeUnique<ArcSessionRunner>(base::Bind(FakeArcSession::Create));
    arc_session_runner_->AddObserver(this);
  }

  void TearDown() override {
    arc_session_runner_->RemoveObserver(this);
    arc_session_runner_.reset();

    chromeos::DBusThreadManager::Shutdown();
  }

  ArcSessionRunner* arc_session_runner() { return arc_session_runner_.get(); }

  FakeArcSession* arc_session() {
    return static_cast<FakeArcSession*>(
        arc_session_runner_->GetArcSessionForTesting());
  }

  StopReason stop_reason() { return stop_reason_; }

  void ResetArcSessionFactory(
      const ArcSessionRunner::ArcSessionFactory& factory) {
    arc_session_runner_->RemoveObserver(this);
    arc_session_runner_ = base::MakeUnique<ArcSessionRunner>(factory);
    arc_session_runner_->AddObserver(this);
  }

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
    // The instance is already destructed in
    // ArcSessionRunner::OnSessionStopped().
    stop_reason_ = stop_reason;
  }

  StopReason stop_reason_;
  std::unique_ptr<ArcSessionRunner> arc_session_runner_;
  base::MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionRunnerTest);
};

// Exercises the basic functionality of the ArcSessionRunner. Observer should
// be notified.
TEST_F(ArcSessionRunnerTest, Basic) {
  class Observer : public ArcSessionObserver {
   public:
    Observer() = default;

    bool ready_called() const { return ready_called_; }
    bool stopped_called() const { return stopped_called_; }

    // ArcSessionObserver:
    void OnSessionReady() override { ready_called_ = true; }
    void OnSessionStopped(StopReason reason) override {
      stopped_called_ = true;
    }

   private:
    bool ready_called_ = false;
    bool stopped_called_ = false;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  Observer observer;
  arc_session_runner()->AddObserver(&observer);
  base::ScopedClosureRunner teardown(base::Bind(
      [](ArcSessionRunner* arc_session_runner, Observer* observer) {
        arc_session_runner->RemoveObserver(observer);
      },
      arc_session_runner(), &observer));

  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());
  EXPECT_TRUE(observer.ready_called());

  arc_session_runner()->RequestStop();
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  EXPECT_TRUE(observer.stopped_called());
}

// If the ArcSessionRunner accepts a request to stop ARC instance, it should
// stop it, even mid-startup.
TEST_F(ArcSessionRunnerTest, StopMidStartup) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateSuspendedArcSession));
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_FALSE(arc_session_runner()->IsStopped());
  EXPECT_FALSE(arc_session_runner()->IsRunning());

  arc_session_runner()->RequestStop();
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

// If the boot procedure is failed, then restarting mechanism should not
// triggered.
TEST_F(ArcSessionRunnerTest, BootFailure) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                 StopReason::GENERIC_BOOT_FAILURE));
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_EQ(StopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

// If the instance is stopped, it should be re-started.
TEST_F(ArcSessionRunnerTest, Restart) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Simulate a connection loss.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(StopReason::CRASH);
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  arc_session_runner()->RequestStop();
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

// Makes sure OnSessionStopped is called on stop.
TEST_F(ArcSessionRunnerTest, OnSessionStopped) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Simulate boot failure.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(StopReason::GENERIC_BOOT_FAILURE);
  EXPECT_EQ(StopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Simulate crash.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(StopReason::CRASH);
  EXPECT_EQ(StopReason::CRASH, stop_reason());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Graceful stop.
  arc_session_runner()->RequestStop();
  EXPECT_EQ(StopReason::SHUTDOWN, stop_reason());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

TEST_F(ArcSessionRunnerTest, Shutdown) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Simulate shutdown.
  arc_session_runner()->OnShutdown();
  EXPECT_EQ(StopReason::SHUTDOWN, stop_reason());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

// Removing the same observer more than once should be okay.
TEST_F(ArcSessionRunnerTest, RemoveObserverTwice) {
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  DummyObserver dummy_observer;
  arc_session_runner()->AddObserver(&dummy_observer);
  // Call RemoveObserver() twice.
  arc_session_runner()->RemoveObserver(&dummy_observer);
  arc_session_runner()->RemoveObserver(&dummy_observer);
}

// Removing an unknown observer should be allowed.
TEST_F(ArcSessionRunnerTest, RemoveUnknownObserver) {
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  DummyObserver dummy_observer;
  arc_session_runner()->RemoveObserver(&dummy_observer);
}

}  // namespace arc
