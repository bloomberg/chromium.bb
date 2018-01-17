// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_session_impl.h"
#include "components/arc/test/fake_arc_bridge_host.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kFakeGmail[] = "user@gmail.com";
constexpr char kFakeGmailGaiaId[] = "1234567890";

class FakeDelegate : public ArcSessionImpl::Delegate {
 public:
  FakeDelegate() = default;

  // Emulates to fail Mojo connection establishing. |callback| passed to
  // ConnectMojo will be called with nullptr.
  void EmulateMojoConnectionFailure() { success_ = false; }

  // Suspends to complete the MojoConnection, when ConnectMojo is called.
  // Later, when ResumeMojoConnection() is called, the passed callback will be
  // asynchronously called.
  void SuspendMojoConnection() { suspend_ = true; }

  // Resumes the pending Mojo connection establishment. Before,
  // SuspendMojoConnection() must be called followed by ConnectMojo().
  // ConnectMojo's |callback| will be called asynchronously.
  void ResumeMojoConnection() {
    DCHECK(!pending_callback_.is_null());
    PostCallback(std::move(pending_callback_));
  }

  // ArcSessionImpl::Delegate override:
  base::ScopedFD ConnectMojo(base::ScopedFD socket_fd,
                             ConnectMojoCallback callback) override {
    if (suspend_) {
      DCHECK(pending_callback_.is_null());
      pending_callback_ = std::move(callback);
    } else {
      PostCallback(std::move(callback));
    }

    // Open /dev/null as a dummy FD.
    return base::ScopedFD(HANDLE_EINTR(open("/dev/null", O_RDONLY)));
  }

 private:
  void PostCallback(ConnectMojoCallback callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            success_ ? std::make_unique<FakeArcBridgeHost>() : nullptr));
  }

  bool success_ = true;
  bool suspend_ = false;
  ConnectMojoCallback pending_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeDelegate);
};

class TestArcSessionObserver : public ArcSession::Observer {
 public:
  struct OnSessionStoppedArgs {
    ArcStopReason reason;
    bool was_running;
  };

  explicit TestArcSessionObserver(ArcSession* arc_session)
      : arc_session_(arc_session) {
    arc_session_->AddObserver(this);
  }

  ~TestArcSessionObserver() override { arc_session_->RemoveObserver(this); }

  const base::Optional<OnSessionStoppedArgs>& on_session_stopped_args() const {
    return on_session_stopped_args_;
  }

  // ArcSession::Observer overrides:
  void OnSessionStopped(ArcStopReason reason, bool was_running) override {
    on_session_stopped_args_.emplace(OnSessionStoppedArgs{reason, was_running});
  }

 private:
  ArcSession* const arc_session_;  // Not owned.
  base::Optional<OnSessionStoppedArgs> on_session_stopped_args_;

  DISALLOW_COPY_AND_ASSIGN(TestArcSessionObserver);
};

// Custom deleter for ArcSession testing.
struct ArcSessionDeleter {
  void operator()(ArcSession* arc_session) {
    // ArcSessionImpl must be in STOPPED state, if the instance is being
    // destroyed. Calling OnShutdown() just before ensures it.
    arc_session->OnShutdown();
    delete arc_session;
  }
};

class ArcSessionImplTest : public testing::Test {
 public:
  ArcSessionImplTest() {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::make_unique<chromeos::FakeSessionManagerClient>());
    GetSessionManagerClient()->set_arc_available(true);

    // Create a user and set it as the primary user.
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kFakeGmail, kFakeGmailGaiaId);
    const user_manager::User* user = GetUserManager()->AddUser(account_id);
    GetUserManager()->UserLoggedIn(account_id, user->username_hash(),
                                   false /* browser_restart */,
                                   false /* is_child */);
  }

  ~ArcSessionImplTest() override {
    GetUserManager()->RemoveUserFromList(
        AccountId::FromUserEmailGaiaId(kFakeGmail, kFakeGmailGaiaId));
    chromeos::DBusThreadManager::Shutdown();
  }

  chromeos::FakeSessionManagerClient* GetSessionManagerClient() {
    return static_cast<chromeos::FakeSessionManagerClient*>(
        chromeos::DBusThreadManager::Get()->GetSessionManagerClient());
  }

  user_manager::FakeUserManager* GetUserManager() {
    return static_cast<user_manager::FakeUserManager*>(
        user_manager::UserManager::Get());
  }

  void EmulateDBusFailure() {
    GetSessionManagerClient()->set_arc_available(false);
  }

  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter> CreateArcSession(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate = nullptr) {
    if (!delegate)
      delegate = std::make_unique<FakeDelegate>();
    return std::unique_ptr<ArcSessionImpl, ArcSessionDeleter>(
        new ArcSessionImpl(std::move(delegate)));
  }

  void SetupMiniContainer(ArcSessionImpl* arc_session,
                          TestArcSessionObserver* observer) {
    arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
              arc_session->GetStateForTesting());
    ASSERT_FALSE(observer->on_session_stopped_args().has_value());
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<user_manager::FakeUserManager>()};

  DISALLOW_COPY_AND_ASSIGN(ArcSessionImplTest);
};

// Simple case. Starting FULL_INSTANCE will eventually start the container.
TEST_F(ArcSessionImplTest, FullInstance_Success) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// SessionManagerClient::StartArcInstance() reports an error, so that
// the container fails to start.
TEST_F(ArcSessionImplTest, FullInstance_DBusFail) {
  EmulateDBusFailure();

  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// SessionManagerClient::StartArcInstance() fails due to low disk, so that
// the container fails to start with reporting LOW_DISK_SPACE.
TEST_F(ArcSessionImplTest, FullInstance_LowDisk) {
  GetSessionManagerClient()->set_low_disk(true);

  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::LOW_DISK_SPACE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Mojo connection fails, so that the container fails to start.
TEST_F(ArcSessionImplTest, FullInstance_MojoConnectionFail) {
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->EmulateMojoConnectionFailure();

  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Starting mini container success case.
TEST_F(ArcSessionImplTest, MiniInstance_Success) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// SessionManagerClient::StartArcInstance() reports an error, so that
// the container fails to start as a mini container.
TEST_F(ArcSessionImplTest, MiniInstance_DBusFail) {
  EmulateDBusFailure();

  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// SessionManagerClient::StartArcInstance() reports an error due to low disk,
// so that the container fails to start as a mini container with reporting
// LOW_DISK_SPACE.
TEST_F(ArcSessionImplTest, MiniInstance_LowDisk) {
  GetSessionManagerClient()->set_low_disk(true);

  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::LOW_DISK_SPACE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Upgrading a mini container to a full container. Success case.
TEST_F(ArcSessionImplTest, Upgrade_Success) {
  // Set up. Start a mini instance.
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then, upgrade to a full instance.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// SessionManagerClient::StartArcInstance() reports an error for upgrading
// case, then the upgrading fails.
TEST_F(ArcSessionImplTest, Upgrade_DBusFail) {
  // Set up. Start a mini instance.
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Hereafter, let SessionManagerClient::StartArcInstance() fail.
  EmulateDBusFailure();

  // Then upgrade, which should fail.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Mojo connection fails on upgrading. Then, the upgrade fails.
TEST_F(ArcSessionImplTest, Upgrade_MojoConnectionFail) {
  // Let Mojo connection fail.
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->EmulateMojoConnectionFailure();

  // Set up. Start mini instance.
  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  // Starting mini instance should succeed, because it is not related to
  // Mojo connection.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Upgrade should fail, due to Mojo connection fail set above.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Calling StartArcInstance() during STARTING_MINI_INSTANCE should eventually
// succeed to run a full container.
TEST_F(ArcSessionImplTest, Upgrade_StartingMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Before moving forward to RUNNING_MINI_INSTANCE, start upgrading it.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);

  // The state should not immediately switch to STARTING_FULL_INSTANCE, yet.
  EXPECT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Complete the upgrade procedure.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// Testing stop during START_MINI_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_StartingMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during RUNNING_MINI_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_RunningMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during STARTING_FULL_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_StartingFullInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_EQ(ArcSessionImpl::State::STARTING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during CONNECTING_MOJO.
TEST_F(ArcSessionImplTest, Stop_ConnectingMojo) {
  // Let Mojo connection suspend.
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->SuspendMojoConnection();
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::CONNECTING_MOJO,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  delegate_ptr->ResumeMojoConnection();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during RUNNING_FULL_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_RunningFullInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during STARTING_FULL_INSTANCE for upgrade.
TEST_F(ArcSessionImplTest, Stop_StartingFullInstanceForUpgrade) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then upgrade.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_EQ(ArcSessionImpl::State::STARTING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to stop during STARTING_FULL_INSTANCE state.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during CONNECTING_MOJO for upgrade.
TEST_F(ArcSessionImplTest, Stop_ConnectingMojoForUpgrade) {
  // Let Mojo connection suspend.
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->SuspendMojoConnection();
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then upgrade. This should suspend at Mojo connection.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::CONNECTING_MOJO,
            arc_session->GetStateForTesting());

  // Request to stop, then resume the Mojo connection.
  arc_session->Stop();
  delegate_ptr->ResumeMojoConnection();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during RUNNING_FULL_INSTANCE after upgrade.
TEST_F(ArcSessionImplTest, Stop_RunningFullInstanceForUpgrade) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // And upgrade successfully.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Then request to stop.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
}

// Testing stop during STARTING_MINI_INSTANCE with upgrade request.
TEST_F(ArcSessionImplTest,
       Stop_StartingFullInstanceForUpgradeDuringMiniInstanceStart) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::MINI_INSTANCE);
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to upgrade during starting mini container.
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  // Then, the state should stay at STARTING_MINI_INSTANCE.
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to stop.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Stop is requested, but at the same time
// SessionManagerClient::StartArcInstance() reports an error. Then, it should
// be handled as regular SHUTDOWN, because graceful shutdown itself is
// difficult and sometimes reports unexpected error although it succeeds.
TEST_F(ArcSessionImplTest, Stop_ConflictWithFailure) {
  // Let SessionManagerClient::StartArcInstance() fail.
  EmulateDBusFailure();

  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  ASSERT_EQ(ArcSessionImpl::State::STARTING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  // Even if D-Bus reports an error, if Stop() is invoked, it will be handled
  // as clean shutdown.
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
}

// Emulating crash.
TEST_F(ArcSessionImplTest, ArcStopInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Deliver the ArcInstanceStopped D-Bus signal.
  auto* session_manager_client = GetSessionManagerClient();
  session_manager_client->NotifyArcInstanceStopped(
      false /* meaning crash */,
      session_manager_client->container_instance_id());

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::CRASH, observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
}

// ArcStopInstance for the *previous* ARC container may be reported
// to the current instance in very racy timing.
// Unrelated ArcStopInstance signal should be ignored.
TEST_F(ArcSessionImplTest, ArcStopInstance_WrongContainerInstanceId) {
  auto arc_session = CreateArcSession();
  arc_session->Start(ArcInstanceMode::FULL_INSTANCE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Deliver the ArcInstanceStopped D-Bus signal.
  auto* session_manager_client = GetSessionManagerClient();
  session_manager_client->NotifyArcInstanceStopped(false /* meaning crash */,
                                                   "dummy instance id");

  // The signal should be ignored.
  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
}

struct PackagesCacheModeState {
  // Possible values for chromeos::switches::kArcPackagesCacheMode
  const char* chrome_switch;
  ArcInstanceMode start_mode;
  login_manager::StartArcInstanceRequest_PackageCacheMode
      expected_packages_cache_mode;
};

constexpr PackagesCacheModeState kPackagesCacheModeStates[] = {
    {nullptr, ArcInstanceMode::FULL_INSTANCE,
     login_manager::StartArcInstanceRequest_PackageCacheMode_DEFAULT},
    {nullptr, ArcInstanceMode::MINI_INSTANCE,
     login_manager::StartArcInstanceRequest_PackageCacheMode_DEFAULT},
    {ArcSessionImpl::kPackagesCacheModeCopy, ArcInstanceMode::FULL_INSTANCE,
     login_manager::StartArcInstanceRequest_PackageCacheMode_COPY_ON_INIT},
    {ArcSessionImpl::kPackagesCacheModeCopy, ArcInstanceMode::MINI_INSTANCE,
     login_manager::StartArcInstanceRequest_PackageCacheMode_DEFAULT},
    {ArcSessionImpl::kPackagesCacheModeSkipCopy, ArcInstanceMode::FULL_INSTANCE,
     login_manager::
         StartArcInstanceRequest_PackageCacheMode_SKIP_SETUP_COPY_ON_INIT},
    {ArcSessionImpl::kPackagesCacheModeCopy, ArcInstanceMode::MINI_INSTANCE,
     login_manager::StartArcInstanceRequest_PackageCacheMode_DEFAULT},
};

class ArcSessionImplPackagesCacheModeTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<PackagesCacheModeState> {};

TEST_P(ArcSessionImplPackagesCacheModeTest, PackagesCacheModes) {
  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter> arc_session =
      CreateArcSession();

  const PackagesCacheModeState& state = GetParam();
  if (state.chrome_switch) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(chromeos::switches::kArcPackagesCacheMode,
                                    state.chrome_switch);
  }

  arc_session->Start(state.start_mode);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state.expected_packages_cache_mode, GetSessionManagerClient()
                                                    ->last_start_arc_request()
                                                    .packages_cache_mode());
}

INSTANTIATE_TEST_CASE_P(,
                        ArcSessionImplPackagesCacheModeTest,
                        ::testing::ValuesIn(kPackagesCacheModeStates));

}  // namespace
}  // namespace arc
