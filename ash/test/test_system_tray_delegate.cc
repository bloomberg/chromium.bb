// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_system_tray_delegate.h"

#include <string>

#include "ash/shell.h"
#include "base/time/time.h"

namespace ash {
namespace test {

TestSystemTrayDelegate::TestSystemTrayDelegate() = default;

TestSystemTrayDelegate::~TestSystemTrayDelegate() = default;

void TestSystemTrayDelegate::SetSessionLengthLimitForTest(
    const base::TimeDelta& new_limit) {
  session_length_limit_ = new_limit;
  session_length_limit_set_ = true;
}

void TestSystemTrayDelegate::ClearSessionLengthLimit() {
  session_length_limit_set_ = false;
}

void TestSystemTrayDelegate::SetCurrentIME(const IMEInfo& info) {
  current_ime_ = info;
}

void TestSystemTrayDelegate::SetAvailableIMEList(const IMEInfoList& list) {
  ime_list_ = list;
}

bool TestSystemTrayDelegate::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  // Just returns TimeTicks::Now(), so the remaining time is always the
  // specified limit. This is useful for testing.
  if (session_length_limit_set_)
    *session_start_time = base::TimeTicks::Now();
  return session_length_limit_set_;
}

bool TestSystemTrayDelegate::GetSessionLengthLimit(
    base::TimeDelta* session_length_limit) {
  if (session_length_limit_set_)
    *session_length_limit = session_length_limit_;
  return session_length_limit_set_;
}

void TestSystemTrayDelegate::GetCurrentIME(IMEInfo* info) {
  *info = current_ime_;
}

void TestSystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {
  *list = ime_list_;
}

}  // namespace test
}  // namespace ash
