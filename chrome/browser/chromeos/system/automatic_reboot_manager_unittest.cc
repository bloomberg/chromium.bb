// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"

#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/test/test_shell_delegate.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

using ::testing::ReturnPointee;

namespace chromeos {
namespace system {

namespace {

// A SingleThreadTaskRunner that mocks the current time and allows it to be
// fast-forwarded. The current time in ticks is returned by Now(). The
// corresponding device uptime is written to |uptime_file_|, providing a mock
// for /proc/uptime.
class MockTimeSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  MockTimeSingleThreadTaskRunner();

  // base::SingleThreadTaskRunner:
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;

  void SetUptimeFile(const base::FilePath& uptime_file);
  void SetUptime(const base::TimeDelta& uptime);

  const base::TimeDelta& Uptime() const;
  const base::TimeTicks& Now() const;

  void FastForwardBy(const base::TimeDelta& delta);
  void FastForwardUntilNoTasksRemain();
  void RunUntilIdle();

 private:
  // Strict weak temporal ordering of tasks.
  class TemporalOrder {
   public:
    bool operator()(
        const std::pair<base::TimeTicks, base::Closure>& first_task,
        const std::pair<base::TimeTicks, base::Closure>& second_task) const;
  };

  virtual ~MockTimeSingleThreadTaskRunner();

  base::FilePath uptime_file_;
  base::TimeDelta uptime_;
  base::TimeTicks now_;
  std::priority_queue<std::pair<base::TimeTicks, base::Closure>,
                      std::vector<std::pair<base::TimeTicks, base::Closure> >,
                      TemporalOrder> tasks_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeSingleThreadTaskRunner);
};

class MockTimeTickClock : public base::TickClock {
 public:
  explicit MockTimeTickClock(
      scoped_refptr<MockTimeSingleThreadTaskRunner> task_runner);
  virtual ~MockTimeTickClock();

  // base::TickClock:
  virtual base::TimeTicks NowTicks() OVERRIDE;

 private:
  scoped_refptr<MockTimeSingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeTickClock);
};

}  // namespace

class AutomaticRebootManagerBasicTest : public testing::Test {
 protected:
  typedef base::OneShotTimer<AutomaticRebootManager> Timer;

  AutomaticRebootManagerBasicTest();
  virtual ~AutomaticRebootManagerBasicTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void SetUpdateRebootNeededUptime(const base::TimeDelta& uptime);
  void SetRebootAfterUpdate(bool reboot_after_update, bool expect_reboot);
  void SetUptimeLimit(const base::TimeDelta& limit, bool expect_reboot);
  void NotifyUpdateRebootNeeded();
  void NotifyResumed(bool expect_reboot);
  void NotifyTerminating(bool expect_reboot);

  void FastForwardBy(const base::TimeDelta& delta, bool expect_reboot);
  void FastForwardUntilNoTasksRemain(bool expect_reboot);

  void CreateAutomaticRebootManager(bool expect_reboot);

  bool ReadUpdateRebootNeededUptimeFromFile(base::TimeDelta* uptime);
  void VerifyLoginScreenIdleTimerIsStopped() const;
  void VerifyNoGracePeriod() const;
  void VerifyGracePeriod(const base::TimeDelta& start_uptime) const;

  bool is_user_logged_in_;
  bool is_logged_in_as_kiosk_app_;

  // The uptime is read in the blocking thread pool and then processed on the
  // UI thread. This causes the UI thread to start processing the uptime when it
  // has increased by a small offset already. The offset is calculated and
  // stored in |uptime_processing_delay_| so that tests can accurately determine
  // the uptime seen by the UI thread.
  base::TimeDelta uptime_processing_delay_;
  base::TimeDelta update_reboot_needed_uptime_;
  base::TimeDelta uptime_limit_;

  scoped_refptr<MockTimeSingleThreadTaskRunner> task_runner_;

  scoped_ptr<AutomaticRebootManager> automatic_reboot_manager_;

 protected:
  FakePowerManagerClient* power_manager_client_;  // Not owned.
  FakeUpdateEngineClient* update_engine_client_;  // Not owned.

  // Sets the status of |update_engine_client_| to NEED_REBOOT for tests.
  void SetUpdateStatusNeedReboot();

 private:
  void VerifyTimerIsStopped(const Timer* timer) const;
  void VerifyTimerIsRunning(const Timer* timer,
                            const base::TimeDelta& delay) const;
  void VerifyLoginScreenIdleTimerIsRunning() const;

  base::ScopedTempDir temp_dir_;
  base::FilePath update_reboot_needed_uptime_file_;

  bool reboot_after_update_;

  base::ThreadTaskRunnerHandle ui_thread_task_runner_handle_;

  TestingPrefServiceSimple local_state_;
  MockUserManager* mock_user_manager_;  // Not owned.
  ScopedUserManagerEnabler user_manager_enabler_;
};

enum AutomaticRebootManagerTestScenario {
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN,
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION,
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION,
};

// This class runs each test case three times:
// * once while the login screen is being shown
// * once while a kiosk app session is in progress
// * once while a non-kiosk-app session is in progress
class AutomaticRebootManagerTest
    : public AutomaticRebootManagerBasicTest,
      public testing::WithParamInterface<AutomaticRebootManagerTestScenario> {
 protected:
  AutomaticRebootManagerTest();
  virtual ~AutomaticRebootManagerTest();
};

void SaveUptimeToFile(const base::FilePath& path,
                      const base::TimeDelta& uptime) {
  if (path.empty() || uptime == base::TimeDelta())
    return;

  const std::string uptime_seconds = base::DoubleToString(uptime.InSecondsF());
  ASSERT_EQ(static_cast<int>(uptime_seconds.size()),
            base::WriteFile(path, uptime_seconds.c_str(),
                            uptime_seconds.size()));
}

MockTimeSingleThreadTaskRunner::MockTimeSingleThreadTaskRunner() {
}

bool MockTimeSingleThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return true;
}

bool MockTimeSingleThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  tasks_.push(std::pair<base::TimeTicks, base::Closure>(now_ + delay, task));
  return true;
}

bool MockTimeSingleThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  NOTREACHED();
  return false;
}

void MockTimeSingleThreadTaskRunner::SetUptimeFile(
    const base::FilePath& uptime_file) {
  uptime_file_ = uptime_file;
  SaveUptimeToFile(uptime_file_, uptime_);
}

void MockTimeSingleThreadTaskRunner::SetUptime(const base::TimeDelta& uptime) {
  uptime_ = uptime;
  SaveUptimeToFile(uptime_file_, uptime_);
}

const base::TimeDelta& MockTimeSingleThreadTaskRunner::Uptime() const {
  return uptime_;
}

const base::TimeTicks& MockTimeSingleThreadTaskRunner::Now() const {
  return now_;
}

void MockTimeSingleThreadTaskRunner::FastForwardBy(
    const base::TimeDelta& delta) {
  const base::TimeTicks latest = now_ + delta;
  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  blocking_pool->FlushForTesting();
  while (!tasks_.empty() && tasks_.top().first <= latest) {
    uptime_ += tasks_.top().first - now_;
    SaveUptimeToFile(uptime_file_, uptime_);
    now_ = tasks_.top().first;
    base::Closure task = tasks_.top().second;
    tasks_.pop();
    task.Run();
    blocking_pool->FlushForTesting();
  }
  uptime_ += latest - now_;
  SaveUptimeToFile(uptime_file_, uptime_);
  now_ = latest;
}

void MockTimeSingleThreadTaskRunner::FastForwardUntilNoTasksRemain() {
  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  blocking_pool->FlushForTesting();
  while (!tasks_.empty()) {
    uptime_ += tasks_.top().first - now_;
    SaveUptimeToFile(uptime_file_, uptime_);
    now_ = tasks_.top().first;
    base::Closure task = tasks_.top().second;
    tasks_.pop();
    task.Run();
    blocking_pool->FlushForTesting();
  }
}

void MockTimeSingleThreadTaskRunner::RunUntilIdle() {
  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  blocking_pool->FlushForTesting();
  while (!tasks_.empty() && tasks_.top().first <= now_) {
    base::Closure task = tasks_.top().second;
    tasks_.pop();
    task.Run();
    blocking_pool->FlushForTesting();
  }
}

bool MockTimeSingleThreadTaskRunner::TemporalOrder::operator()(
    const std::pair<base::TimeTicks, base::Closure>& first_task,
    const std::pair<base::TimeTicks, base::Closure>& second_task) const {
  return first_task.first > second_task.first;
}

MockTimeSingleThreadTaskRunner::~MockTimeSingleThreadTaskRunner() {
}

MockTimeTickClock::MockTimeTickClock(
    scoped_refptr<MockTimeSingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

MockTimeTickClock::~MockTimeTickClock() {
}

base::TimeTicks MockTimeTickClock::NowTicks() {
  return task_runner_->Now();
}

AutomaticRebootManagerBasicTest::AutomaticRebootManagerBasicTest()
    : is_user_logged_in_(false),
      is_logged_in_as_kiosk_app_(false),
      task_runner_(new MockTimeSingleThreadTaskRunner),
      power_manager_client_(NULL),
      update_engine_client_(NULL),
      reboot_after_update_(false),
      ui_thread_task_runner_handle_(task_runner_),
      mock_user_manager_(new MockUserManager),
      user_manager_enabler_(mock_user_manager_) {
}

AutomaticRebootManagerBasicTest::~AutomaticRebootManagerBasicTest() {
}

void AutomaticRebootManagerBasicTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath& temp_dir = temp_dir_.path();
  const base::FilePath uptime_file = temp_dir.Append("uptime");
  task_runner_->SetUptimeFile(uptime_file);
  ASSERT_FALSE(base::WriteFile(uptime_file, NULL, 0));
  update_reboot_needed_uptime_file_ =
      temp_dir.Append("update_reboot_needed_uptime");
  ASSERT_FALSE(base::WriteFile(update_reboot_needed_uptime_file_, NULL, 0));
  ASSERT_TRUE(PathService::Override(chromeos::FILE_UPTIME, uptime_file));
  ASSERT_TRUE(PathService::Override(chromeos::FILE_UPDATE_REBOOT_NEEDED_UPTIME,
                                    update_reboot_needed_uptime_file_));

  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  AutomaticRebootManager::RegisterPrefs(local_state_.registry());

  scoped_ptr<DBusThreadManagerSetter> dbus_setter =
      chromeos::DBusThreadManager::GetSetterForTesting();
  power_manager_client_ = new FakePowerManagerClient;
  dbus_setter->SetPowerManagerClient(
      scoped_ptr<PowerManagerClient>(power_manager_client_));
  update_engine_client_ = new FakeUpdateEngineClient;
  dbus_setter->SetUpdateEngineClient(
      scoped_ptr<UpdateEngineClient>(update_engine_client_));

  EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
     .WillRepeatedly(ReturnPointee(&is_user_logged_in_));
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
     .WillRepeatedly(ReturnPointee(&is_logged_in_as_kiosk_app_));
}

void AutomaticRebootManagerBasicTest::TearDown() {
  // Let the AutomaticRebootManager, if any, unregister itself as an observer of
  // several subsystems.
  automatic_reboot_manager_.reset();
  task_runner_->RunUntilIdle();

  DBusThreadManager::Shutdown();
  TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
}

void AutomaticRebootManagerBasicTest::SetUpdateRebootNeededUptime(
    const base::TimeDelta& uptime) {
  update_reboot_needed_uptime_ = uptime;
  SaveUptimeToFile(update_reboot_needed_uptime_file_, uptime);
}


void AutomaticRebootManagerBasicTest::SetRebootAfterUpdate(
    bool reboot_after_update,
    bool expect_reboot) {
  reboot_after_update_ = reboot_after_update;
  local_state_.SetManagedPref(prefs::kRebootAfterUpdate,
                              new base::FundamentalValue(reboot_after_update));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            power_manager_client_->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::SetUptimeLimit(
    const base::TimeDelta& limit,
    bool expect_reboot) {
  uptime_limit_ = limit;
  if (limit == base::TimeDelta()) {
    local_state_.RemoveManagedPref(prefs::kUptimeLimit);
  } else {
    local_state_.SetManagedPref(
        prefs::kUptimeLimit,
        new base::FundamentalValue(static_cast<int>(limit.InSeconds())));
  }
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            power_manager_client_->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyUpdateRebootNeeded() {
  SetUpdateStatusNeedReboot();
  automatic_reboot_manager_->UpdateStatusChanged(
      update_engine_client_->GetLastStatus());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyResumed(bool expect_reboot) {
  automatic_reboot_manager_->SuspendDone(base::TimeDelta::FromHours(1));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            power_manager_client_->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyTerminating(bool expect_reboot) {
  automatic_reboot_manager_->Observe(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::Source<AutomaticRebootManagerBasicTest>(this),
      content::NotificationService::NoDetails());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            power_manager_client_->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::FastForwardBy(
    const base::TimeDelta& delta,
    bool expect_reboot) {
  task_runner_->FastForwardBy(delta);
  EXPECT_EQ(expect_reboot ? 1 : 0,
            power_manager_client_->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::FastForwardUntilNoTasksRemain(
    bool expect_reboot) {
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            power_manager_client_->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::CreateAutomaticRebootManager(
    bool expect_reboot) {
  automatic_reboot_manager_.reset(new AutomaticRebootManager(
      scoped_ptr<base::TickClock>(new MockTimeTickClock(task_runner_))));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            power_manager_client_->num_request_restart_calls());

  uptime_processing_delay_ =
      base::TimeTicks() - automatic_reboot_manager_->boot_time_ -
      task_runner_->Uptime();
  EXPECT_GE(uptime_processing_delay_, base::TimeDelta());
  EXPECT_LE(uptime_processing_delay_, base::TimeDelta::FromSeconds(1));

  if (is_user_logged_in_ || expect_reboot)
    VerifyLoginScreenIdleTimerIsStopped();
  else
    VerifyLoginScreenIdleTimerIsRunning();
}

bool AutomaticRebootManagerBasicTest::ReadUpdateRebootNeededUptimeFromFile(
    base::TimeDelta* uptime) {
  std::string contents;
  if (!base::ReadFileToString(update_reboot_needed_uptime_file_, &contents)) {
    return false;
  }
  double seconds;
  if (!base::StringToDouble(contents.substr(0, contents.find(' ')), &seconds) ||
      seconds < 0.0) {
    return false;
  }
  *uptime = base::TimeDelta::FromMilliseconds(seconds * 1000.0);
  return true;
}

void AutomaticRebootManagerBasicTest::
    VerifyLoginScreenIdleTimerIsStopped() const {
  VerifyTimerIsStopped(
      automatic_reboot_manager_->login_screen_idle_timer_.get());
}

void AutomaticRebootManagerBasicTest::VerifyNoGracePeriod() const {
  EXPECT_FALSE(automatic_reboot_manager_->reboot_requested_);
  VerifyTimerIsStopped(automatic_reboot_manager_->grace_start_timer_.get());
  VerifyTimerIsStopped(automatic_reboot_manager_->grace_end_timer_.get());
}

void AutomaticRebootManagerBasicTest::VerifyGracePeriod(
    const base::TimeDelta& start_uptime) const {
  const base::TimeDelta start =
      start_uptime - task_runner_->Uptime() - uptime_processing_delay_;
  const base::TimeDelta end = start + base::TimeDelta::FromHours(24);
  if (start <= base::TimeDelta()) {
    EXPECT_TRUE(automatic_reboot_manager_->reboot_requested_);
    VerifyTimerIsStopped(automatic_reboot_manager_->grace_start_timer_.get());
    VerifyTimerIsRunning(automatic_reboot_manager_->grace_end_timer_.get(),
                         end);
  } else {
    EXPECT_FALSE(automatic_reboot_manager_->reboot_requested_);
    VerifyTimerIsRunning(automatic_reboot_manager_->grace_start_timer_.get(),
                         start);
    VerifyTimerIsRunning(automatic_reboot_manager_->grace_end_timer_.get(),
                         end);
  }
}

void AutomaticRebootManagerBasicTest::VerifyTimerIsStopped(
    const Timer* timer) const {
  if (timer)
    EXPECT_FALSE(timer->IsRunning());
}

void AutomaticRebootManagerBasicTest::VerifyTimerIsRunning(
    const Timer* timer,
    const base::TimeDelta& delay) const {
  ASSERT_TRUE(timer);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(delay.ToInternalValue(),
            timer->GetCurrentDelay().ToInternalValue());
}

void AutomaticRebootManagerBasicTest::
    VerifyLoginScreenIdleTimerIsRunning() const {
  VerifyTimerIsRunning(
      automatic_reboot_manager_->login_screen_idle_timer_.get(),
      base::TimeDelta::FromSeconds(60));
}

void AutomaticRebootManagerBasicTest::SetUpdateStatusNeedReboot() {
  UpdateEngineClient::Status client_status;
  client_status.status = UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
  update_engine_client_->set_default_status(client_status);
}

AutomaticRebootManagerTest::AutomaticRebootManagerTest() {
  switch (GetParam()) {
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN:
      is_user_logged_in_ = false;
      is_logged_in_as_kiosk_app_ = false;
      break;
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION:
      is_user_logged_in_ = true;
      is_logged_in_as_kiosk_app_ = true;
      break;
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION:
      is_user_logged_in_ = true;
      is_logged_in_as_kiosk_app_ = false;
      break;
  }
}

AutomaticRebootManagerTest::~AutomaticRebootManagerTest() {
}

// Chrome is showing the login screen. The current uptime is 12 hours.
// Verifies that the idle timer is running. Further verifies that when a kiosk
// app session begins, the idle timer is stopped.
TEST_F(AutomaticRebootManagerBasicTest, LoginStopsIdleTimer) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately and the login screen
  // idle timer is started.
  CreateAutomaticRebootManager(false);

  // Notify that a kiosk app session has been started.
  is_user_logged_in_ = true;
  is_logged_in_as_kiosk_app_ = true;
  automatic_reboot_manager_->Observe(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<AutomaticRebootManagerBasicTest>(this),
      content::NotificationService::NoDetails());

  // Verify that the login screen idle timer is stopped.
  VerifyLoginScreenIdleTimerIsStopped();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is showing the login screen. The current uptime is 12 hours.
// Verifies that the idle timer is running. Further verifies that when a
// non-kiosk-app session begins, the idle timer is stopped.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskLoginStopsIdleTimer) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately and the login screen
  // idle timer is started.
  CreateAutomaticRebootManager(false);

  // Notify that a non-kiosk-app session has been started.
  is_user_logged_in_ = true;
  automatic_reboot_manager_->Observe(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<AutomaticRebootManagerBasicTest>(this),
      content::NotificationService::NoDetails());

  // Verify that the login screen idle timer is stopped.
  VerifyLoginScreenIdleTimerIsStopped();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is showing the login screen. The uptime limit is 6 hours. The current
// uptime is 12 hours.
// Verifies that user activity prevents the device from rebooting. Further
// verifies that when user activity ceases, the devices reboots.
TEST_F(AutomaticRebootManagerBasicTest, UserActivityResetsIdleTimer) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately and the login screen
  // idle timer is started.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 25 minutes while simulating user activity every
  // 50 seconds.
  for (int i = 0; i < 30; ++i) {
    // Fast forward uptime by 50 seconds. Verify that the device does not reboot
    // immediately.
    FastForwardBy(base::TimeDelta::FromSeconds(50), false);

    // Simulate user activity.
    automatic_reboot_manager_->OnUserActivity(NULL);
  }

  // Fast forward the uptime by 60 seconds without simulating user activity.
  // Verify that the device reboots immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(60), true);
}

// Chrome is running a kiosk app session. The current uptime is 10 days.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, ResumeNoPolicy) {
  is_user_logged_in_ = true;
  is_logged_in_as_kiosk_app_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a non-kiosk-app session. The current uptime is 10 days.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeAppNoPolicy) {
  is_user_logged_in_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 24 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, ResumeBeforeGracePeriod) {
  is_user_logged_in_ = true;
  is_logged_in_as_kiosk_app_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device eventually reboots.
  FastForwardUntilNoTasksRemain(true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 24 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeBeforeGracePeriod) {
  is_user_logged_in_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 6 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it immediately
// reboots.
TEST_F(AutomaticRebootManagerBasicTest, ResumeInGracePeriod) {
  is_user_logged_in_ = true;
  is_logged_in_as_kiosk_app_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device reboots immediately.
  NotifyResumed(true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 6 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeInGracePeriod) {
  is_user_logged_in_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 6 hours. The
// current uptime is 29 hours 30 minutes.
// Verifies that when the device is suspended and then resumes, it immediately
// reboots.
TEST_F(AutomaticRebootManagerBasicTest, ResumeAfterGracePeriod) {
  is_user_logged_in_ = true;
  is_logged_in_as_kiosk_app_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromHours(29) +
                          base::TimeDelta::FromMinutes(30));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device reboots immediately.
  NotifyResumed(true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 6 hours. The
// current uptime is 29 hours 30 minutes.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeAfterGracePeriod) {
  is_user_logged_in_ = true;
  task_runner_->SetUptime(base::TimeDelta::FromHours(29) +
                          base::TimeDelta::FromMinutes(30));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The current uptime is 10 days.
// Verifies that when the browser terminates, the device does not immediately
// reboot.
TEST_P(AutomaticRebootManagerTest, TerminateNoPolicy) {
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the browser is terminating. Verify that the device does not
  // reboot immediately.
  NotifyTerminating(false);

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 12 hours.
// Verifies that when the browser terminates, it does not immediately reboot.
TEST_P(AutomaticRebootManagerTest, TerminateBeforeGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the browser is terminating. Verify that the device does not
  // reboot immediately.
  NotifyTerminating(false);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The uptime limit is set to 6 hours. The current uptime is
// 12 hours.
// Verifies that when the browser terminates, the device immediately reboots if
// a kiosk app session is in progress.
TEST_P(AutomaticRebootManagerTest, TerminateInGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the browser is terminating. Verify that the device immediately
  // reboots if a kiosk app session is in progress.
  NotifyTerminating(is_logged_in_as_kiosk_app_);

  // Verify that if a non-kiosk-app session is in progress, the device does not
  // reboot eventually.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when the uptime limit is set to 24 hours, no reboot occurs and
// a grace period is scheduled to begin after 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, BeforeUptimeLimitGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when the uptime limit is set to 6 hours, a reboot is requested
// and a grace period is started that will end after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, InUptimeLimitGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The current uptime is 10 days.
// Verifies that when the uptime limit is set to 6 hours, the device reboots
// immediately if no non-kiosk-app-session is in progress because the grace
// period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, AfterUptimeLimitGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Set the uptime limit. Verify that unless a non-kiosk-app session is in
  // progress, the the device immediately reboots.
  SetUptimeLimit(base::TimeDelta::FromHours(6), !is_user_logged_in_ ||
                                                is_logged_in_as_kiosk_app_);

  // Verify that if a non-kiosk-app session is in progress, the device does not
  // reboot eventually.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 6 hours.
// Verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, UptimeLimitOffBeforeGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 1 hour. Verify that the device does not reboot
  // immediately.
  FastForwardBy(base::TimeDelta::FromHours(1), false);

  // Remove the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 24 hours.
// Verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, UptimeLimitOffInGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(24));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Remove the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 6 hours.
// Verifies that when the uptime limit is extended to 24 hours, the grace period
// is rescheduled to start further in the future.
TEST_P(AutomaticRebootManagerTest, ExtendUptimeLimitBeforeGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Extend the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that the grace period has been rescheduled to start further in the
  // future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 18 hours.
// Verifies that when the uptime limit is extended to 24 hours, the grace period
// is rescheduled to start in the future.
TEST_P(AutomaticRebootManagerTest, ExtendUptimeLimitInGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(18));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Extend the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that the grace period has been rescheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The uptime limit is set to 18 hours. The current uptime is
// 12 hours.
// Verifies that when the uptime limit is shortened to 6 hours, the grace period
// is rescheduled to have already started.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitBeforeToInGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(18), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Shorten the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that the grace period has been rescheduled and has started already.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 36 hours.
// Verifies that when the uptime limit is shortened to 18 hours, the grace
// period is rescheduled to have started earlier.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitInToInGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(36));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Shorten the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(18), false);

  // Verify that the grace period has been rescheduled to have started earlier.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 36 hours.
// Verifies that when the uptime limit is shortened to 6 hours, the device
// reboots immediately if no non-kiosk-app session is in progress because the
// grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitInToAfterGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(36));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Shorten the uptime limit. Verify that unless a non-kiosk-app session is in
  // progress, the the device immediately reboots.
  SetUptimeLimit(base::TimeDelta::FromHours(6), !is_user_logged_in_ ||
                                                is_logged_in_as_kiosk_app_);

  // Verify that if a non-kiosk-app session is in progress, the device does not
  // reboot eventually.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when an update is applied, the current uptime is persisted as
// the time at which a reboot became necessary. Further verifies that when the
// policy to automatically reboot after an update is not enabled, no reboot
// occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, UpdateNoPolicy) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when an update is applied, the current uptime is persisted as
// the time at which a reboot became necessary. Further verifies that when the
// policy to automatically reboot after an update is enabled, a reboot is
// requested and a grace period is started that will end 24 hours from now.
TEST_P(AutomaticRebootManagerTest, Update) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when Chrome is notified twice that an update has been applied,
// the second notification is ignored and the uptime at which it occured does
// not get persisted as the time at which an update became necessary.
TEST_P(AutomaticRebootManagerTest, UpdateAfterUpdate) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the previously persisted time at which a reboot became
  // necessary has not been overwritten.
  base::TimeDelta new_update_reboot_needed_uptime;
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &new_update_reboot_needed_uptime));
  EXPECT_EQ(update_reboot_needed_uptime_, new_update_reboot_needed_uptime);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The current uptime is 10 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, no reboot occurs a grace period is scheduled to begin after the
// minimum of 1 hour of uptime. Further verifies that when an update is applied,
// the current uptime is persisted as the time at which a reboot became
// necessary.
TEST_P(AutomaticRebootManagerTest, UpdateBeforeMinimumUptime) {
  task_runner_->SetUptime(base::TimeDelta::FromMinutes(10));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has been scheduled to begin in the future.
  VerifyGracePeriod(base::TimeDelta::FromHours(1));

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, PolicyAfterUpdateInGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Fast forward the uptime to 12 hours. Verify that the device does not reboot
  // immediately.
  FastForwardBy(base::TimeDelta::FromHours(6), false);

  // Simulate user activity.
  automatic_reboot_manager_->OnUserActivity(NULL);

  // Enable automatic reboot after an update has been applied. Verify that the
  // device does not reboot immediately.
  SetRebootAfterUpdate(true, false);

  // Verify that a grace period has started.
  VerifyGracePeriod(base::TimeDelta::FromHours(6) + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is
// enabled, the device reboots immediately if no non-kiosk-app session is in
// progress because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, PolicyAfterUpdateAfterGracePeriod) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Fast forward the uptime to 12 hours. Verify that the device does not reboot
  // immediately.
  FastForwardBy(base::TimeDelta::FromDays(10) - base::TimeDelta::FromHours(6),
                false);

  // Simulate user activity.
  automatic_reboot_manager_->OnUserActivity(NULL);

  // Enable automatic rebooting after an update has been applied. Verify that
  // unless a non-kiosk-app session is in progress, the the device immediately
  // reboots.
  SetRebootAfterUpdate(true, !is_user_logged_in_ || is_logged_in_as_kiosk_app_);

  // Verify that if a non-kiosk-app session is in progress, the device does not
  // reboot eventually.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The policy to
// automatically reboot after an update is enabled. The current uptime is
// 6 hours 20 seconds.
// Verifies that when the policy to automatically reboot after an update is
// disabled, the reboot request and grace period are removed.
TEST_P(AutomaticRebootManagerTest, PolicyOffAfterUpdate) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(6));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that a grace period has started.
  VerifyGracePeriod(task_runner_->Uptime() + uptime_processing_delay_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Disable automatic rebooting after an update has been applied. Verify that
  // the device does not reboot immediately.
  SetRebootAfterUpdate(false, false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The current uptime is not available.
// Verifies that even if an uptime limit is set, the policy to automatically
// reboot after an update is enabled and an update has been applied, no reboot
// occurs and no grace period is scheduled. Further verifies that no time is
// persisted as the time at which a reboot became necessary.
TEST_P(AutomaticRebootManagerTest, NoUptime) {
  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Enable automatic rebooting after an update has been applied. Verify that
  // the device does not reboot immediately.
  SetRebootAfterUpdate(true, false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that no time is persisted as the time at which a reboot became
  // necessary.
  EXPECT_FALSE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The policy to automatically reboot after an update is
// enabled. The current uptime is 12 hours.
// Verifies that when an uptime limit of 6 hours is set, the availability of an
// update does not cause the grace period to be rescheduled. Further verifies
// that the current uptime is persisted as the time at which a reboot became
// necessary.
TEST_P(AutomaticRebootManagerTest, UptimeLimitBeforeUpdate) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that the grace period has not been rescheduled.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The policy to automatically reboot after an update is
// enabled. The current uptime is 12 hours.
// Verifies that when an uptime limit of 24 hours is set, the availability of an
// update causes the grace period to be rescheduled so that it ends 24 hours
// from now. Further verifies that the current uptime is persisted as the time
// at which a reboot became necessary.
TEST_P(AutomaticRebootManagerTest, UpdateBeforeUptimeLimit) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that the grace period has been rescheduled to start at the time that
  // the update became available.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is running. The uptime limit is set to 24 hours. An update was applied
// and a reboot became necessary to complete the update process after 12 hours.
// The policy to automatically reboot after an update is enabled. The current
// uptime is 12 hours 20 seconds.
// Verifies that when the policy to reboot after an update is disabled, the
// grace period is rescheduled to start after 24 hours of uptime. Further
// verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, PolicyOffThenUptimeLimitOff) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that the grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has been rescheduled to end 24 hours from now.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Disable automatic reboot after an update has been applied. Verify that the
  // device does not reboot immediately.
  SetRebootAfterUpdate(false, false);

  // Verify that the grace period has been rescheduled to start after 24 hours
  // of uptime.
  VerifyGracePeriod(uptime_limit_);

  // Remove the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 6 hours. An update was applied
// and a reboot became necessary to complete the update process after 12 hours.
// The policy to automatically reboot after an update is enabled. The current
// uptime is 12 hours 20 seconds.
// Verifies that when the uptime limit is removed, the grace period is
// rescheduled to have started after 12 hours of uptime. Further verifies that
// when the policy to reboot after an update is disabled, the reboot request and
// grace period are removed.
TEST_P(AutomaticRebootManagerTest, UptimeLimitOffThenPolicyOff) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that the grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that the grace period has been rescheduled to have started after
  // 6 hours of uptime.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Remove the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that a grace period has been rescheduled to have started after 12
  // hours of uptime.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Disable automatic reboot after an update has been applied. Verify that the
  // device does not reboot immediately.
  SetRebootAfterUpdate(false, false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is 6 hours. The current uptime is
// 29 hours 59 minutes 59 seconds.
// Verifies that if no non-kiosk-app session is in progress, the device reboots
// immediately when the grace period ends after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, GracePeriodEnd) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(29) +
                          base::TimeDelta::FromMinutes(59) +
                          base::TimeDelta::FromSeconds(59));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Set the uptime limit. Verify that the device does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 1 second. Verify that unless a non-kiosk-app
  // session is in progress, the the device immediately reboots.
  FastForwardBy(base::TimeDelta::FromSeconds(1), !is_user_logged_in_ ||
                                                 is_logged_in_as_kiosk_app_);

  // Verify that if a non-kiosk-app session is in progress, the device does not
  // reboot eventually.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. The current uptime is 10 days.
// Verifies that when no automatic reboot policy is enabled, no reboot occurs
// and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoPolicy) {
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. The uptime limit is set to 24 hours. The current uptime
// is 12 hours.
// Verifies that no reboot occurs and a grace period is scheduled to begin after
// 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartBeforeUptimeLimitGracePeriod) {
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. The uptime limit is set to 6 hours. The current uptime is
// 10 days.
// Verifies that if no non-kiosk-app session is in progress, the device reboots
// immediately because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartAfterUptimeLimitGracePeriod) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that unless a non-kiosk-app session is in progress, the the device
  // immediately reboots.
  CreateAutomaticRebootManager(!is_user_logged_in_ ||
                               is_logged_in_as_kiosk_app_);

  // Verify that if a non-kiosk-app session is in progress, the device does not
  // reboot eventually.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. The uptime limit is set to 6 hours. The current uptime is
// 12 hours.
// Verifies that a reboot is requested and a grace period is started that will
// end after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartInUptimeLimitGracePeriod) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is
// enabled, the device reboots immediately if no non-kiosk-app session is in
// progress because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartAfterUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));
  SetRebootAfterUpdate(true, false);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // reboots immediately.
  CreateAutomaticRebootManager(!is_user_logged_in_ ||
                               is_logged_in_as_kiosk_app_);

  // Verify that if a non-kiosk-app session is in progress, the device does not
  // reboot eventually.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartInUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 10 minutes of uptime. The current uptime is
// 20 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, no reboot occurs and a grace period is scheduled to begin after the
// minimum of 1 hour of uptime.
TEST_P(AutomaticRebootManagerTest, StartBeforeUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromMinutes(10));
  task_runner_->SetUptime(base::TimeDelta::FromMinutes(20));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(base::TimeDelta::FromHours(1));

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is not
// enabled, no reboot occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartUpdateNoPolicy) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process but the time at which this happened was lost. The
// current uptime is 10 days.
// Verifies that the current uptime is persisted as the time at which a reboot
// became necessary. Further verifies that when the policy to automatically
// reboot after an update is enabled, a reboot is requested and a grace period
// is started that will end 24 hours from now.
TEST_P(AutomaticRebootManagerTest, StartUpdateTimeLost) {
  SetUpdateStatusNeedReboot();
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process but the time at which this happened was lost. The
// current uptime is 10 days.
// Verifies that the current uptime is persisted as the time at which a reboot
// became necessary. Further verifies that when the policy to automatically
// reboot after an update is not enabled, no reboot occurs and no grace period
// is scheduled.
TEST_P(AutomaticRebootManagerTest, StartUpdateNoPolicyTimeLost) {
  SetUpdateStatusNeedReboot();
  task_runner_->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(task_runner_->Uptime(), update_reboot_needed_uptime_);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. No update has been applied. The current uptime is
// 12 hours.
// Verifies that no time is persisted as the time at which a reboot became
// necessary. Further verifies that no reboot occurs and no grace period is
// scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoUpdate) {
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no time is persisted as the time at which a reboot became
  // necessary.
  EXPECT_FALSE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. The uptime limit is set to 6 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 8 hours of uptime. The current uptime is 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartUptimeLimitBeforeUpdate) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(8));
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. The uptime limit is set to 8 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 6 hours of uptime. The current uptime is 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartUpdateBeforeUptimeLimit) {
  SetUptimeLimit(base::TimeDelta::FromHours(8), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  task_runner_->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ ||
                                is_logged_in_as_kiosk_app_);
}

// Chrome is starting. The uptime limit is set to 6 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 6 hours of uptime. The current uptime is not available.
// Verifies that even if the policy to automatically reboot after an update is
// enabled, no reboot occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoUptime) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  SetRebootAfterUpdate(true, false);

  // Verify that the device does not reboot immediately.
  CreateAutomaticRebootManager(false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that the device does not reboot eventually.
  FastForwardUntilNoTasksRemain(false);
}

INSTANTIATE_TEST_CASE_P(
    AutomaticRebootManagerTestInstance,
    AutomaticRebootManagerTest,
    ::testing::Values(
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN,
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION,
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION));

}  // namespace system
}  // namespace chromeos
