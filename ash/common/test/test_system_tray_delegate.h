// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
#define ASH_COMMON_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

#include "ash/common/system/tray/default_system_tray_delegate.h"
#include "ash/common/system/tray/ime_info.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ash {
namespace test {

class TestSystemTrayDelegate : public DefaultSystemTrayDelegate {
 public:
  TestSystemTrayDelegate();
  ~TestSystemTrayDelegate() override;

  // Changes the current login status in the test. This also invokes
  // UpdateAfterLoginStatusChange(). Usually this is called in the test code to
  // set up a login status. This will fit to most of the test cases, but this
  // cannot be set during the initialization. To test the initialization,
  // consider using SetInitialLoginStatus() instead.
  void SetLoginStatus(LoginStatus login_status);

  // Updates the session length limit so that the limit will come from now in
  // |new_limit|.
  void SetSessionLengthLimitForTest(const base::TimeDelta& new_limit);

  // Clears the session length limit.
  void ClearSessionLengthLimit();

  // Sets the IME info.
  void SetCurrentIME(const IMEInfo& info);

  // Sets the list of available IMEs.
  void SetAvailableIMEList(const IMEInfoList& list);

  // Overridden from SystemTrayDelegate:
  LoginStatus GetUserLoginStatus() const override;
  bool IsUserSupervised() const override;
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  void GetCurrentIME(IMEInfo* info) override;
  void GetAvailableIMEList(IMEInfoList* list) override;

 private:
  LoginStatus login_status_;
  base::TimeDelta session_length_limit_;
  bool session_length_limit_set_;
  IMEInfo current_ime_;
  IMEInfoList ime_list_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayDelegate);
};

// Changes the initial login status before TestSystemTrayDelegate is created.
// Allows testing the case when chrome is restarted right after login (such as
// when a flag is set).
class ScopedInitialLoginStatus {
 public:
  explicit ScopedInitialLoginStatus(LoginStatus status);
  ~ScopedInitialLoginStatus();

 private:
  LoginStatus old_status_;

  DISALLOW_COPY_AND_ASSIGN(ScopedInitialLoginStatus);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_COMMON_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
