// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/logout_button/logout_button_tray.h"

#include <queue>
#include <utility>
#include <vector>

#include "ash/system/status_area_widget.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {
namespace internal {

class LogoutConfirmationDialogTest;

namespace {

// A SingleThreadTaskRunner that mocks the current time and allows it to be
// fast-forwarded.
// TODO: crbug.com/329911
// Move shared copies of this class to one place.
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

  const base::TimeTicks& GetCurrentTime() const;

  void FastForwardBy(base::TimeDelta delta);
  void FastForwardUntilNoTasksRemain();

 private:
  // Strict weak temporal ordering of tasks.
  class TemporalOrder {
   public:
    bool operator()(
        const std::pair<base::TimeTicks, base::Closure>& first_task,
        const std::pair<base::TimeTicks, base::Closure>& second_task) const;
  };

  virtual ~MockTimeSingleThreadTaskRunner();

  base::TimeTicks now_;
  std::priority_queue<std::pair<base::TimeTicks, base::Closure>,
                      std::vector<std::pair<base::TimeTicks, base::Closure> >,
                      TemporalOrder> tasks_;
};

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

const base::TimeTicks& MockTimeSingleThreadTaskRunner::GetCurrentTime() const {
  return now_;
}

void MockTimeSingleThreadTaskRunner::FastForwardBy(base::TimeDelta delta) {
  const base::TimeTicks latest = now_ + delta;
  while (!tasks_.empty() && tasks_.top().first <= latest) {
    now_ = tasks_.top().first;
    base::Closure task = tasks_.top().second;
    tasks_.pop();
    task.Run();
  }
  now_ = latest;
}

void MockTimeSingleThreadTaskRunner::FastForwardUntilNoTasksRemain() {
  while (!tasks_.empty()) {
    now_ = tasks_.top().first;
    base::Closure task = tasks_.top().second;
    tasks_.pop();
    task.Run();
  }
}

bool MockTimeSingleThreadTaskRunner::TemporalOrder::operator()(
    const std::pair<base::TimeTicks, base::Closure>& first_task,
    const std::pair<base::TimeTicks, base::Closure>& second_task) const {
  return first_task.first > second_task.first;
}

MockTimeSingleThreadTaskRunner::~MockTimeSingleThreadTaskRunner() {
}

}  // namespace

class MockLogoutConfirmationDelegate
    : public LogoutConfirmationDialogView::Delegate {
 public:
  MockLogoutConfirmationDelegate(
      scoped_refptr<MockTimeSingleThreadTaskRunner> runner,
      LogoutConfirmationDialogTest* tester);

  // LogoutConfirmationDialogView::Delegate:
  virtual void LogoutCurrentUser() OVERRIDE;
  virtual base::TimeTicks GetCurrentTime() const OVERRIDE;
  virtual void ShowDialog(views::DialogDelegate* dialog) OVERRIDE;

  bool logout_called() const { return logout_called_; }

 private:
  bool logout_called_;

  scoped_refptr<MockTimeSingleThreadTaskRunner> runner_;
  scoped_ptr<views::DialogDelegate> dialog_ownership_holder_;
  LogoutConfirmationDialogTest* tester_;

  DISALLOW_COPY_AND_ASSIGN(MockLogoutConfirmationDelegate);
};

class LogoutConfirmationDialogTest : public testing::Test {
 public:
  LogoutConfirmationDialogTest();
  virtual ~LogoutConfirmationDialogTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;

  void ChangeDialogDuration(base::TimeDelta duration);

  void CloseDialog();
  bool IsDialogShowing();
  void PressButton();
  void PressDialogButtonYes();

 protected:
  scoped_ptr<LogoutButtonTray> logout_button_;
  scoped_refptr<MockTimeSingleThreadTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handle_;
  MockLogoutConfirmationDelegate* delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoutConfirmationDialogTest);
};

MockLogoutConfirmationDelegate::MockLogoutConfirmationDelegate(
    scoped_refptr<MockTimeSingleThreadTaskRunner> runner,
    LogoutConfirmationDialogTest* tester)
    : logout_called_(false),
      runner_(runner),
      tester_(tester) {
}

void MockLogoutConfirmationDelegate::LogoutCurrentUser() {
  logout_called_ = true;
  tester_->CloseDialog();
}

base::TimeTicks MockLogoutConfirmationDelegate::GetCurrentTime() const {
  return runner_->GetCurrentTime();
}

void MockLogoutConfirmationDelegate::ShowDialog(views::DialogDelegate* dialog) {
  // Simulate the ownership passing of dialog in tests.
  dialog_ownership_holder_.reset(dialog);
}

LogoutConfirmationDialogTest::LogoutConfirmationDialogTest()
    : runner_(new MockTimeSingleThreadTaskRunner),
      runner_handle_(runner_) {
}

LogoutConfirmationDialogTest::~LogoutConfirmationDialogTest() {
}

void LogoutConfirmationDialogTest::SetUp() {
  logout_button_.reset(new LogoutButtonTray(NULL));
  delegate_ = new MockLogoutConfirmationDelegate(runner_, this);
  logout_button_->SetDelegateForTest(
      scoped_ptr<LogoutConfirmationDialogView::Delegate>(delegate_));
  ChangeDialogDuration(base::TimeDelta::FromSeconds(20));
}

void LogoutConfirmationDialogTest::ChangeDialogDuration(
    base::TimeDelta duration) {
  logout_button_->OnLogoutDialogDurationChanged(duration);
}

void LogoutConfirmationDialogTest::CloseDialog() {
  if (logout_button_->confirmation_dialog_)
    logout_button_->confirmation_dialog_->OnClosed();
}

bool LogoutConfirmationDialogTest::IsDialogShowing() {
  return logout_button_->IsConfirmationDialogShowing();
}

void LogoutConfirmationDialogTest::PressButton() {
  const ui::TranslatedKeyEvent faked_event(
      false,
      static_cast<ui::KeyboardCode>(0),
      0);
  logout_button_->ButtonPressed(
      reinterpret_cast<views::Button*>(logout_button_->button_), faked_event);
}

void LogoutConfirmationDialogTest::PressDialogButtonYes() {
  logout_button_->confirmation_dialog_->Accept();
  // |confirmation_dialog_| might already be destroyed, if not, manually call
  // OnClosed() to simulate real browser environment behavior.
  if (logout_button_->confirmation_dialog_)
    logout_button_->confirmation_dialog_->OnClosed();
}

TEST_F(LogoutConfirmationDialogTest, NoClickWithDefaultValue) {
  PressButton();

  // Verify that the dialog is showing immediately after the logout button was
  // pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(0));
  EXPECT_TRUE(IsDialogShowing());
  EXPECT_FALSE(delegate_->logout_called());

  // Verify that the dialog is still showing after 19 seconds since the logout
  // button was pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(19));
  EXPECT_TRUE(IsDialogShowing());
  EXPECT_FALSE(delegate_->logout_called());

  // Verify that the dialog is closed after 21 seconds since the logout button
  // was pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_FALSE(IsDialogShowing());
  EXPECT_TRUE(delegate_->logout_called());
}

TEST_F(LogoutConfirmationDialogTest, ZeroPreferenceValue) {
  ChangeDialogDuration(base::TimeDelta::FromSeconds(0));

  EXPECT_FALSE(IsDialogShowing());

  PressButton();

  // Verify that user was logged out immediately after the logout button was
  // pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(0));
  EXPECT_FALSE(IsDialogShowing());
  EXPECT_TRUE(delegate_->logout_called());

  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(IsDialogShowing());
}

TEST_F(LogoutConfirmationDialogTest, OnTheFlyDialogDurationChange) {
  ChangeDialogDuration(base::TimeDelta::FromSeconds(5));

  EXPECT_FALSE(IsDialogShowing());

  PressButton();

  // Verify that the dialog is showing immediately after the logout button was
  // pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(0));
  EXPECT_TRUE(IsDialogShowing());
  EXPECT_FALSE(delegate_->logout_called());

  // Verify that the dialog is still showing after 3 seconds since the logout
  // button was pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(3));
  EXPECT_TRUE(IsDialogShowing());
  EXPECT_FALSE(delegate_->logout_called());

  // And at this point we change the dialog duration preference.
  ChangeDialogDuration(base::TimeDelta::FromSeconds(10));

  // Verify that the dialog is still showing after 9 seconds since the logout
  // button was pressed, with dialog duration preference changed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(6));
  EXPECT_TRUE(IsDialogShowing());
  EXPECT_FALSE(delegate_->logout_called());

  // Verify that the dialog is closed after 11 seconds since the logout button
  // was pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_FALSE(IsDialogShowing());
  EXPECT_TRUE(delegate_->logout_called());
}

TEST_F(LogoutConfirmationDialogTest, UserClickedButton) {
  PressButton();

  // Verify that the dialog is showing immediately after the logout button was
  // pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(0));
  EXPECT_TRUE(IsDialogShowing());
  EXPECT_FALSE(delegate_->logout_called());

  // Verify that the dialog is still showing after 3 seconds since the logout
  // button was pressed.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(3));
  EXPECT_TRUE(IsDialogShowing());
  EXPECT_FALSE(delegate_->logout_called());

  // And at this point we click the accept button.
  PressDialogButtonYes();

  // Verify that the user was logged out immediately after the accept button
  // was clicked.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(0));
  EXPECT_TRUE(delegate_->logout_called());
}

}  // namespace internal
}  // namespace ash
