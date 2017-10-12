// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class DoNothingObserver : public ArcSessionRunner::Observer {
 public:
  void OnSessionStopped(ArcStopReason reason, bool restarting) override {
    // Do nothing.
  }
  void OnSessionRestarting() override {
    // Do nothing.
  }
};

}  // namespace

class ArcSessionRunnerTest : public testing::Test,
                             public ArcSessionRunner::Observer {
 public:
  ArcSessionRunnerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::make_unique<chromeos::FakeSessionManagerClient>());
    chromeos::DBusThreadManager::Initialize();

    stop_reason_ = ArcStopReason::SHUTDOWN;
    restarting_ = false;
    stopped_called_ = false;
    restarting_called_ = false;

    // We inject FakeArcSession here so we do not need task_runner.
    arc_session_runner_ =
        std::make_unique<ArcSessionRunner>(base::Bind(FakeArcSession::Create));
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

  ArcStopReason stop_reason() {
    EXPECT_TRUE(stopped_called());
    return stop_reason_;
  }

  bool restarting() {
    EXPECT_TRUE(stopped_called());
    return restarting_;
  }

  bool stopped_called() { return stopped_called_; }
  bool restarting_called() { return restarting_called_; }

  void ResetArcSessionFactory(
      const ArcSessionRunner::ArcSessionFactory& factory) {
    arc_session_runner_->RemoveObserver(this);
    arc_session_runner_ = std::make_unique<ArcSessionRunner>(factory);
    arc_session_runner_->AddObserver(this);
  }

  static std::unique_ptr<ArcSession> CreateSuspendedArcSession() {
    auto arc_session = std::make_unique<FakeArcSession>();
    arc_session->SuspendBoot();
    return std::move(arc_session);
  }

  static std::unique_ptr<ArcSession> CreateBootFailureArcSession(
      ArcStopReason reason) {
    auto arc_session = std::make_unique<FakeArcSession>();
    arc_session->EnableBootFailureEmulation(reason);
    return std::move(arc_session);
  }

 private:
  // ArcSessionRunner::Observer:
  void OnSessionStopped(ArcStopReason stop_reason, bool restarting) override {
    // The instance is already destructed in
    // ArcSessionRunner::OnSessionStopped().
    stop_reason_ = stop_reason;
    restarting_ = restarting;
    stopped_called_ = true;
    restarting_called_ = false;
  }
  void OnSessionRestarting() override { restarting_called_ = true; }

  ArcStopReason stop_reason_;
  bool restarting_;
  bool stopped_called_;
  bool restarting_called_;
  std::unique_ptr<ArcSessionRunner> arc_session_runner_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionRunnerTest);
};

// Exercises the basic functionality of the ArcSessionRunner. Observer should
// be notified.
TEST_F(ArcSessionRunnerTest, Basic) {
  class Observer : public ArcSessionRunner::Observer {
   public:
    Observer() = default;

    bool stopped_called() const { return stopped_called_; }

    // ArcSessionRunner::Observer:
    void OnSessionStopped(ArcStopReason reason, bool restarting) override {
      stopped_called_ = true;
    }
    void OnSessionRestarting() override {}

   private:
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

  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->IsRunning());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
  EXPECT_TRUE(observer.stopped_called());
}

// If the ArcSessionRunner accepts a request to stop ARC instance, it should
// stop it, even mid-startup.
TEST_F(ArcSessionRunnerTest, StopMidStartup) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateSuspendedArcSession));
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->IsRunning());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(restarting());
}

// Does the same for mini instance.
TEST_F(ArcSessionRunnerTest, StopMidStartup_MiniInstance) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateSuspendedArcSession));
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::MINI_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->IsRunning());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
}

// If the boot procedure is failed, then restarting mechanism should not
// triggered.
TEST_F(ArcSessionRunnerTest, BootFailure) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                 ArcStopReason::GENERIC_BOOT_FAILURE));
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(restarting());
}

// Does the same with the mini instance.
TEST_F(ArcSessionRunnerTest, BootFailure_MiniInstance) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                 ArcStopReason::GENERIC_BOOT_FAILURE));
  EXPECT_FALSE(arc_session());

  // If starting the mini instance fails, arc_session_runner()'s state goes back
  // to STOPPED, but its observers won't be notified.
  arc_session_runner()->RequestStart(ArcInstanceMode::MINI_INSTANCE);
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(stopped_called());

  // Also make sure that RequestStart() works just fine after the boot
  // failure.
  ResetArcSessionFactory(base::Bind(FakeArcSession::Create));
  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->IsRunning());
}

// Similary, CRASH should do same for GENERIC_BOOT_FAILURE case, because
// in mini instance, Mojo connection should not be established.
TEST_F(ArcSessionRunnerTest, Crash_MiniInstance) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                 ArcStopReason::CRASH));
  EXPECT_FALSE(arc_session());

  // If starting the mini instance fails, arc_session_runner()'s state goes back
  // to STOPPED, but its observers won't be notified.
  arc_session_runner()->RequestStart(ArcInstanceMode::MINI_INSTANCE);
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(stopped_called());
}

// Tests that RequestStart() works even after EmitLoginPromptVisibleCalled()
// is called.
TEST_F(ArcSessionRunnerTest, Upgrade) {
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::MINI_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->IsRunning());

  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->IsRunning());
}

// We expect mini instance starts to run if EmitLoginPromptVisible signal is
// emitted.
TEST_F(ArcSessionRunnerTest, EmitLoginPromptVisible) {
  EXPECT_FALSE(arc_session());

  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->EmitLoginPromptVisible();
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->IsRunning());
}

// If the instance is stopped, it should be re-started.
TEST_F(ArcSessionRunnerTest, Restart) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->IsRunning());

  // Simulate a connection loss.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(ArcStopReason::CRASH);
  EXPECT_FALSE(arc_session());
  EXPECT_TRUE(restarting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(restarting_called());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->IsRunning());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
}

TEST_F(ArcSessionRunnerTest, GracefulStop) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->IsRunning());

  // Graceful stop.
  arc_session_runner()->RequestStop();
  EXPECT_EQ(ArcStopReason::SHUTDOWN, stop_reason());
  EXPECT_FALSE(restarting());
  EXPECT_FALSE(restarting_called());
  EXPECT_FALSE(arc_session());
}

TEST_F(ArcSessionRunnerTest, Shutdown) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStart(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->IsRunning());

  // Simulate shutdown.
  arc_session_runner()->OnShutdown();
  EXPECT_EQ(ArcStopReason::SHUTDOWN, stop_reason());
  EXPECT_FALSE(arc_session());
}

// Removing the same observer more than once should be okay.
TEST_F(ArcSessionRunnerTest, RemoveObserverTwice) {
  EXPECT_FALSE(arc_session());

  DoNothingObserver do_nothing_observer;
  arc_session_runner()->AddObserver(&do_nothing_observer);
  // Call RemoveObserver() twice.
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
}

// Removing an unknown observer should be allowed.
TEST_F(ArcSessionRunnerTest, RemoveUnknownObserver) {
  EXPECT_FALSE(arc_session());

  DoNothingObserver do_nothing_observer;
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
}

}  // namespace arc
