// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_win.h"

#include <userenv.h>

#include <algorithm>

#include "base/logging.h"
#include "base/object_watcher.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/policy_constants.h"

namespace policy {

// Period at which to run the reload task in case the group policy change
// watchers fail.
const int kReloadIntervalMinutes = 15;

ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::
    GroupPolicyChangeWatcher(
        base::WeakPtr<ConfigurationPolicyProviderWin> provider,
        int reload_interval_minutes)
        : provider_(provider),
          user_policy_changed_event_(false, false),
          machine_policy_changed_event_(false, false),
          user_policy_watcher_failed_(false),
          machine_policy_watcher_failed_(false),
          reload_interval_minutes_(reload_interval_minutes),
          reload_task_(NULL) {
  if (!RegisterGPNotification(user_policy_changed_event_.handle(), false)) {
    PLOG(WARNING) << "Failed to register user group policy notification";
    user_policy_watcher_failed_ = true;
  }
  if (!RegisterGPNotification(machine_policy_changed_event_.handle(), true)) {
    PLOG(WARNING) << "Failed to register machine group policy notification.";
    machine_policy_watcher_failed_ = true;
  }
}

ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::
    ~GroupPolicyChangeWatcher() {
  if (MessageLoop::current())
    MessageLoop::current()->RemoveDestructionObserver(this);
  DCHECK(!reload_task_);
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::Start() {
  MessageLoop::current()->AddDestructionObserver(this);
  SetupWatches();
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::Stop() {
  user_policy_watcher_.StopWatching();
  machine_policy_watcher_.StopWatching();
  if (reload_task_) {
    reload_task_->Cancel();
    reload_task_ = NULL;
  }
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::SetupWatches() {
  if (reload_task_) {
    reload_task_->Cancel();
    reload_task_ = NULL;
  }

  if (!user_policy_watcher_failed_) {
    if (!user_policy_watcher_.GetWatchedObject() &&
        !user_policy_watcher_.StartWatching(
            user_policy_changed_event_.handle(), this)) {
      LOG(WARNING) << "Failed to start watch for user policy change event";
      user_policy_watcher_failed_ = true;
    }
  }
  if (!machine_policy_watcher_failed_) {
    if (!machine_policy_watcher_.GetWatchedObject() &&
        !machine_policy_watcher_.StartWatching(
            machine_policy_changed_event_.handle(), this)) {
      LOG(WARNING) << "Failed to start watch for machine policy change event";
      machine_policy_watcher_failed_ = true;
    }
  }

  if (user_policy_watcher_failed_ || machine_policy_watcher_failed_) {
    reload_task_ =
        NewRunnableMethod(this, &GroupPolicyChangeWatcher::ReloadFromTask);
    int64 delay =
        base::TimeDelta::FromMinutes(reload_interval_minutes_).InMilliseconds();
    MessageLoop::current()->PostDelayedTask(FROM_HERE, reload_task_, delay);
  }
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::Reload() {
  if (provider_.get())
    provider_->NotifyStoreOfPolicyChange();
  SetupWatches();
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::
    ReloadFromTask() {
  // Make sure to not call Cancel() on the task, since it might hold the last
  // reference that keeps this object alive.
  reload_task_ = NULL;
  Reload();
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::
    OnObjectSignaled(HANDLE object) {
  DCHECK(object == user_policy_changed_event_.handle() ||
         object == machine_policy_changed_event_.handle());
  Reload();
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::
    WillDestroyCurrentMessageLoop() {
  reload_task_ = NULL;
  MessageLoop::current()->RemoveDestructionObserver(this);
}

ConfigurationPolicyProviderWin::ConfigurationPolicyProviderWin(
    const StaticPolicyValueMap& policy_map)
    : ConfigurationPolicyProvider(policy_map) {
  watcher_ = new GroupPolicyChangeWatcher(this->AsWeakPtr(),
                                          kReloadIntervalMinutes);
  watcher_->Start();
}

ConfigurationPolicyProviderWin::~ConfigurationPolicyProviderWin() {
  watcher_->Stop();
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyString(
    const string16& name, string16* result) {
  string16 path = string16(kRegistrySubKey);
  RegKey policy_key;
  // First try the global policy.
  if (policy_key.Open(HKEY_LOCAL_MACHINE, path.c_str(), KEY_READ)) {
    if (ReadRegistryStringValue(&policy_key, name, result))
      return true;
    policy_key.Close();
  }
  // Fall back on user-specific policy.
  if (!policy_key.Open(HKEY_CURRENT_USER, path.c_str(), KEY_READ))
    return false;
  return ReadRegistryStringValue(&policy_key, name, result);
}

bool ConfigurationPolicyProviderWin::ReadRegistryStringValue(
    RegKey* key, const string16& name, string16* result) {
  DWORD value_size = 0;
  DWORD key_type = 0;
  scoped_array<uint8> buffer;

  if (!key->ReadValue(name.c_str(), 0, &value_size, &key_type))
    return false;
  if (key_type != REG_SZ)
    return false;

  // According to the Microsoft documentation, the string
  // buffer may not be explicitly 0-terminated. Allocate a
  // slightly larger buffer and pre-fill to zeros to guarantee
  // the 0-termination.
  buffer.reset(new uint8[value_size + 2]);
  memset(buffer.get(), 0, value_size + 2);
  key->ReadValue(name.c_str(), buffer.get(), &value_size, NULL);
  result->assign(reinterpret_cast<const wchar_t*>(buffer.get()));
  return true;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyStringList(
    const string16& key, ListValue* result) {
  string16 path = string16(kRegistrySubKey);
  path += ASCIIToUTF16("\\") + key;
  RegKey policy_key;
  if (!policy_key.Open(HKEY_LOCAL_MACHINE, path.c_str(), KEY_READ)) {
    policy_key.Close();
    // Fall back on user-specific policy.
    if (!policy_key.Open(HKEY_CURRENT_USER, path.c_str(), KEY_READ))
      return false;
  }
  string16 policy_string;
  int index = 0;
  while (ReadRegistryStringValue(&policy_key, base::IntToString16(++index),
                                 &policy_string)) {
    result->Append(Value::CreateStringValue(policy_string));
  }
  return true;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyBoolean(
    const string16& value_name, bool* result) {
  DWORD value;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, kRegistrySubKey, KEY_READ);
  if (hkcu_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value != 0;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, kRegistrySubKey, KEY_READ);
  if (hklm_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value != 0;
    return true;
  }
  return false;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyInteger(
    const string16& value_name, uint32* result) {
  DWORD value;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, kRegistrySubKey, KEY_READ);
  if (hkcu_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, kRegistrySubKey, KEY_READ);
  if (hklm_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value;
    return true;
  }
  return false;
}

bool ConfigurationPolicyProviderWin::Provide(
    ConfigurationPolicyStore* store) {
  const PolicyValueMap& mapping(policy_value_map());
  for (PolicyValueMap::const_iterator current = mapping.begin();
       current != mapping.end(); ++current) {
    std::wstring name = UTF8ToWide(current->name);
    switch (current->value_type) {
      case Value::TYPE_STRING: {
        std::wstring string_value;
        if (GetRegistryPolicyString(name.c_str(), &string_value)) {
          store->Apply(current->policy_type,
                       Value::CreateStringValue(string_value));
        }
        break;
      }
      case Value::TYPE_LIST: {
        scoped_ptr<ListValue> list_value(new ListValue);
        if (GetRegistryPolicyStringList(name.c_str(), list_value.get()))
          store->Apply(current->policy_type, list_value.release());
        break;
      }
      case Value::TYPE_BOOLEAN: {
        bool bool_value;
        if (GetRegistryPolicyBoolean(name.c_str(), &bool_value)) {
          store->Apply(current->policy_type,
                       Value::CreateBooleanValue(bool_value));
        }
        break;
      }
      case Value::TYPE_INTEGER: {
        uint32 int_value;
        if (GetRegistryPolicyInteger(name.c_str(), &int_value)) {
          store->Apply(current->policy_type,
                       Value::CreateIntegerValue(int_value));
        }
        break;
      }
      default:
        NOTREACHED();
        return false;
    }
  }

  return true;
}

}  // namespace policy
