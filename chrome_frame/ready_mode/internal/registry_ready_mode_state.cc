// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/internal/registry_ready_mode_state.h"

#include "base/time.h"
#include "base/task.h"
#include "base/win/registry.h"
#include "chrome_frame/ready_mode/internal/installation_state.h"
#include "chrome_frame/ready_mode/ready_mode_manager.h"

namespace {

const wchar_t kReadyModeStateValue[] = L"ReadyModeState";

};  // namespace

RegistryReadyModeState::RegistryReadyModeState(
    const std::wstring& key_name, base::TimeDelta temporary_decline_duration,
    InstallationState* installation_state, Observer* observer)
    : key_name_(key_name),
      temporary_decline_duration_(temporary_decline_duration),
      installation_state_(installation_state),
      observer_(observer) {
}

RegistryReadyModeState::~RegistryReadyModeState() {
}

base::Time RegistryReadyModeState::GetNow() {
  return base::Time::Now();
}

ReadyModeStatus RegistryReadyModeState::GetStatus() {
  if (installation_state_->IsProductInstalled())
    return READY_MODE_ACCEPTED;

  if (!installation_state_->IsProductRegistered())
    return READY_MODE_PERMANENTLY_DECLINED;

  bool exists = false;
  int64 value = 0;

  if (!GetValue(&value, &exists))
    return READY_MODE_TEMPORARILY_DECLINED;

  if (!exists)
    return READY_MODE_ACTIVE;

  if (value == 0)
    return READY_MODE_PERMANENTLY_DECLINED;

  base::Time when_declined(base::Time::FromInternalValue(value));
  base::Time now(GetNow());

  // If the decline duration has passed, or is further in the future than
  // the total timeout, consider it expired.
  bool expired = (now - when_declined) > temporary_decline_duration_ ||
      (when_declined - now) > temporary_decline_duration_;

  // To avoid a race-condition in GetValue (between ValueExists and ReadValue)
  // we never delete the temporary decline flag.
  if (expired)
    return READY_MODE_ACTIVE;

  return READY_MODE_TEMPORARILY_DECLINED;
}

bool RegistryReadyModeState::GetValue(int64* value, bool* exists) {
  *exists = false;
  *value = 0;

  base::win::RegKey config_key;
  if (!config_key.Open(HKEY_CURRENT_USER, key_name_.c_str(), KEY_QUERY_VALUE)) {
    DLOG(ERROR) << "Failed to open registry key " << key_name_;
    return false;
  }

  if (!config_key.ValueExists(kReadyModeStateValue))
    return true;

  int64 temp;
  DWORD value_size = sizeof(temp);
  DWORD type = 0;
  if (!config_key.ReadValue(kReadyModeStateValue, &temp, &value_size, &type)) {
    DLOG(ERROR) << "Failed to open registry key " << key_name_;
    return false;
  }

  if (value_size != sizeof(temp) || type != REG_QWORD) {
    DLOG(ERROR) << "Unexpected state found under registry key " << key_name_
                << " and value " << kReadyModeStateValue;
    config_key.DeleteValue(kReadyModeStateValue);
    return true;
  }

  *value = temp;
  *exists = true;
  return true;
}

bool RegistryReadyModeState::StoreValue(int64 value) {
  base::win::RegKey config_key;
  if (config_key.Open(HKEY_CURRENT_USER, key_name_.c_str(), KEY_SET_VALUE) &&
      config_key.WriteValue(kReadyModeStateValue, &value, sizeof(value),
                            REG_QWORD)) {
    return true;
  }

  DLOG(ERROR) << "Failed to open or write to registry key " << key_name_
              << " and value " << kReadyModeStateValue;

  return false;
}

void RegistryReadyModeState::TemporarilyDeclineChromeFrame() {
  int64 value = GetNow().ToInternalValue();

  if (StoreValue(value))
    observer_->OnStateChange();
}

void RegistryReadyModeState::PermanentlyDeclineChromeFrame() {
  bool success = false;

  // Either change, by itself, will deactivate Ready Mode, though we prefer to
  // also unregister, in order to free up resources.

  if (StoreValue(0))
    success = true;

  if (installation_state_->UnregisterProduct())
    success = true;

  if (success)
    observer_->OnStateChange();
}

void RegistryReadyModeState::AcceptChromeFrame() {
  if (installation_state_->InstallProduct())
    observer_->OnStateChange();
}
