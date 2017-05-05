// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_delegate.h"

#include "ash/system/tray/ime_info.h"
#include "ash/system/tray/system_tray_item.h"

namespace ash {

SystemTrayDelegate::SystemTrayDelegate() {}

SystemTrayDelegate::~SystemTrayDelegate() {}

void SystemTrayDelegate::Initialize() {}

void SystemTrayDelegate::ShowUserLogin() {}

void SystemTrayDelegate::GetCurrentIME(IMEInfo* info) {}

void SystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {}

void SystemTrayDelegate::GetCurrentIMEProperties(IMEPropertyInfoList* list) {}

base::string16 SystemTrayDelegate::GetIMEManagedMessage() {
  return base::string16();
}

void SystemTrayDelegate::SwitchIME(const std::string& ime_id) {}

void SystemTrayDelegate::ActivateIMEProperty(const std::string& key) {}

NetworkingConfigDelegate* SystemTrayDelegate::GetNetworkingConfigDelegate()
    const {
  return nullptr;
}

bool SystemTrayDelegate::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  return false;
}

bool SystemTrayDelegate::GetSessionLengthLimit(
    base::TimeDelta* session_length_limit) {
  return false;
}

void SystemTrayDelegate::ActiveUserWasChanged() {}

bool SystemTrayDelegate::IsSearchKeyMappedToCapsLock() {
  return false;
}

std::unique_ptr<SystemTrayItem> SystemTrayDelegate::CreateRotationLockTrayItem(
    SystemTray* tray) {
  return nullptr;
}

}  // namespace ash
