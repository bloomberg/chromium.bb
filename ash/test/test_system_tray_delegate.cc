// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_system_tray_delegate.h"

namespace ash {
namespace test {

TestSystemTrayDelegate::TestSystemTrayDelegate() = default;

TestSystemTrayDelegate::~TestSystemTrayDelegate() = default;

void TestSystemTrayDelegate::SetCurrentIME(const IMEInfo& info) {
  current_ime_ = info;
}

void TestSystemTrayDelegate::SetAvailableIMEList(const IMEInfoList& list) {
  ime_list_ = list;
}

void TestSystemTrayDelegate::GetCurrentIME(IMEInfo* info) {
  *info = current_ime_;
}

void TestSystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {
  *list = ime_list_;
}

}  // namespace test
}  // namespace ash
