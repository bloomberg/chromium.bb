// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
#define ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

#include "ash/system/tray/ime_info.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/macros.h"

namespace ash {
namespace test {

class TestSystemTrayDelegate : public SystemTrayDelegate {
 public:
  TestSystemTrayDelegate();
  ~TestSystemTrayDelegate() override;

  // Sets the IME info.
  void SetCurrentIME(const IMEInfo& info);

  // Sets the list of available IMEs.
  void SetAvailableIMEList(const IMEInfoList& list);

  // SystemTrayDelegate:
  void GetCurrentIME(IMEInfo* info) override;
  void GetAvailableIMEList(IMEInfoList* list) override;

 private:
  IMEInfo current_ime_;
  IMEInfoList ime_list_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
