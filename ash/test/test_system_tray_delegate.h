// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
#define ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

#include "ash/system/tray/default_system_tray_delegate.h"
#include "base/time/time.h"

namespace ash {
namespace test {

class TestSystemTrayDelegate : public DefaultSystemTrayDelegate {
 public:
  TestSystemTrayDelegate();
  virtual ~TestSystemTrayDelegate();

  // Changes the login status when initially the delegate is created. This will
  // be called before AshTestBase::SetUp() to test the case when chrome is
  // restarted right after the login (such like a flag is set).
  // This value will be reset in AshTestHelper::TearDown,  most test fixtures
  // don't need to care its lifecycle.
  static void SetInitialLoginStatus(user::LoginStatus login_status);

  // Changes the current login status in the test. This also invokes
  // UpdateAfterLoginStatusChange(). Usually this is called in the test code to
  // set up a login status. This will fit to most of the test cases, but this
  // cannot be set during the initialization. To test the initialization,
  // consider using SetInitialLoginStatus() instead.
  void SetLoginStatus(user::LoginStatus login_status);

  void set_should_show_display_notification(bool should_show) {
    should_show_display_notification_ = should_show;
  }

  // Updates the session length limit so that the limit will come from now in
  // |new_limit|.
  void SetSessionLengthLimitForTest(const base::TimeDelta& new_limit);

  // Clears the session length limit.
  void ClearSessionLengthLimit();

  // Overridden from SystemTrayDelegate:
  virtual user::LoginStatus GetUserLoginStatus() const OVERRIDE;
  virtual bool IsUserSupervised() const OVERRIDE;
  virtual bool ShouldShowDisplayNotification() OVERRIDE;
  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) OVERRIDE;
  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) OVERRIDE;
  virtual void ShutDown() OVERRIDE;
  virtual void SignOut() OVERRIDE;

 private:
  bool should_show_display_notification_;
  user::LoginStatus login_status_;
  base::TimeDelta session_length_limit_;
  bool session_length_limit_set_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
