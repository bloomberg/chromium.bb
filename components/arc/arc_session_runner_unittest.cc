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
        base::MakeUnique<chromeos::FakeSessionManagerClient>());
    chromeos::DBusThreadManager::Initialize();

    stop_reason_ = ArcStopReason::SHUTDOWN;
    restarting_ = false;
    stopped_called_ = false;
    restarting_called_ = false;

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
    arc_session_runner_ = base::MakeUnique<ArcSessionRunner>(factory);
    arc_session_runner_->AddObserver(this);
  }

  static std::unique_ptr<ArcSession> CreateSuspendedArcSession() {
    auto arc_session = base::MakeUnique<FakeArcSession>();
    arc_session->SuspendBoot();
    return std::move(arc_session);
  }

  static std::unique_ptr<ArcSession> CreateBootFailureArcSession(
      ArcStopReason reason) {
    auto arc_session = base::MakeUnique<FakeArcSession>();
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

  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  arc_session_runner()->RequestStop(false);
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

  arc_session_runner()->RequestStop(false);
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

// If the boot procedure is failed, then restarting mechanism should not
// triggered.
TEST_F(ArcSessionRunnerTest, BootFailure) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                 ArcStopReason::GENERIC_BOOT_FAILURE));
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

// Does the same with the mini instance for login screen.
// TODO(yusukes): Enable the test once EmitLoginPromptVisibleCalled() is fully
// enabled.
TEST_F(ArcSessionRunnerTest, DISABLED_BootFailureForLoginScreen) {
  ResetArcSessionFactory(
      base::Bind(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                 ArcStopReason::CRASH));
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->EmitLoginPromptVisible();
  // If starting the mini instance fails, arc_session_runner()'s state goes back
  // to STOPPED, but its observers won't be notified.
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  EXPECT_FALSE(stopped_called());

  // Also make sure that RequestStart() works just fine after the boot
  // failure.
  ResetArcSessionFactory(base::Bind(FakeArcSession::Create));
  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());
}

// Tests that RequestStart() works even after EmitLoginPromptVisibleCalled()
// is called.
// TODO(yusukes): Enable the test once EmitLoginPromptVisibleCalled() is fully
// enabled.
TEST_F(ArcSessionRunnerTest, DISABLED_StartWithLoginScreenInstance) {
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->EmitLoginPromptVisible();
  EXPECT_FALSE(arc_session_runner()->IsStopped());
  EXPECT_FALSE(arc_session_runner()->IsRunning());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());
}

// If the instance is stopped, it should be re-started.
TEST_F(ArcSessionRunnerTest, Restart) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Simulate a connection loss.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(ArcStopReason::CRASH);
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  arc_session_runner()->RequestStop(false);
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
  arc_session()->StopWithReason(ArcStopReason::GENERIC_BOOT_FAILURE);
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_TRUE(restarting());
  EXPECT_FALSE(restarting_called());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(restarting_called());
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Simulate crash.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(ArcStopReason::CRASH);
  EXPECT_EQ(ArcStopReason::CRASH, stop_reason());
  EXPECT_TRUE(restarting());
  EXPECT_FALSE(restarting_called());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(restarting_called());
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Graceful stop.
  arc_session_runner()->RequestStop(false);
  EXPECT_EQ(ArcStopReason::SHUTDOWN, stop_reason());
  EXPECT_FALSE(restarting());
  EXPECT_FALSE(restarting_called());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

TEST_F(ArcSessionRunnerTest, Shutdown) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  arc_session_runner()->RequestStart();
  EXPECT_TRUE(arc_session_runner()->IsRunning());

  // Simulate shutdown.
  arc_session_runner()->OnShutdown();
  EXPECT_EQ(ArcStopReason::SHUTDOWN, stop_reason());
  EXPECT_TRUE(arc_session_runner()->IsStopped());
}

// Removing the same observer more than once should be okay.
TEST_F(ArcSessionRunnerTest, RemoveObserverTwice) {
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  DoNothingObserver do_nothing_observer;
  arc_session_runner()->AddObserver(&do_nothing_observer);
  // Call RemoveObserver() twice.
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
}

// Removing an unknown observer should be allowed.
TEST_F(ArcSessionRunnerTest, RemoveUnknownObserver) {
  EXPECT_TRUE(arc_session_runner()->IsStopped());

  DoNothingObserver do_nothing_observer;
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
}

}  // namespace arc
