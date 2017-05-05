// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
#define ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

#include "ash/system/tray/ime_info.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ash {
namespace test {

class TestSystemTrayDelegate : public SystemTrayDelegate {
 public:
  TestSystemTrayDelegate();
  ~TestSystemTrayDelegate() override;

  // Updates the session length limit so that the limit will come from now in
  // |new_limit|.
  void SetSessionLengthLimitForTest(const base::TimeDelta& new_limit);

  // Clears the session length limit.
  void ClearSessionLengthLimit();

  // Sets the IME info.
  void SetCurrentIME(const IMEInfo& info);

  // Sets the list of available IMEs.
  void SetAvailableIMEList(const IMEInfoList& list);

  // SystemTrayDelegate:
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  void GetCurrentIME(IMEInfo* info) override;
  void GetAvailableIMEList(IMEInfoList* list) override;

 private:
  base::TimeDelta session_length_limit_;
  bool session_length_limit_set_ = false;
  IMEInfo current_ime_;
  IMEInfoList ime_list_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
