// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/system_salt_getter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
namespace {

SystemSaltGetter* g_system_salt_getter = NULL;

}  // namespace

SystemSaltGetter::SystemSaltGetter() : weak_ptr_factory_(this) {
}

SystemSaltGetter::~SystemSaltGetter() {
}

void SystemSaltGetter::GetSystemSalt(
    const GetSystemSaltCallback& callback) {
  if (!system_salt_.empty()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, system_salt_));
    return;
  }

  DBusThreadManager::Get()->GetCryptohomeClient()->WaitForServiceToBeAvailable(
      base::Bind(&SystemSaltGetter::DidWaitForServiceToBeAvailable,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void SystemSaltGetter::DidWaitForServiceToBeAvailable(
    const GetSystemSaltCallback& callback,
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "WaitForServiceToBeAvailable failed.";
    callback.Run(std::string());
    return;
  }
  DBusThreadManager::Get()->GetCryptohomeClient()->GetSystemSalt(
      base::Bind(&SystemSaltGetter::DidGetSystemSalt,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void SystemSaltGetter::DidGetSystemSalt(const GetSystemSaltCallback& callback,
                                        DBusMethodCallStatus call_status,
                                        const std::vector<uint8>& system_salt) {
  if (call_status == DBUS_METHOD_CALL_SUCCESS &&
      !system_salt.empty() &&
      system_salt.size() % 2 == 0U)
    system_salt_ = ConvertRawSaltToHexString(system_salt);
  else
    LOG(WARNING) << "System salt not available";

  callback.Run(system_salt_);
}

// static
void SystemSaltGetter::Initialize() {
  CHECK(!g_system_salt_getter);
  g_system_salt_getter = new SystemSaltGetter();
}

// static
bool SystemSaltGetter::IsInitialized() {
  return g_system_salt_getter;
}

// static
void SystemSaltGetter::Shutdown() {
  CHECK(g_system_salt_getter);
  delete g_system_salt_getter;
  g_system_salt_getter = NULL;
}

// static
SystemSaltGetter* SystemSaltGetter::Get() {
  CHECK(g_system_salt_getter)
      << "SystemSaltGetter::Get() called before Initialize()";
  return g_system_salt_getter;
}

// static
std::string SystemSaltGetter::ConvertRawSaltToHexString(
    const std::vector<uint8>& salt) {
  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(salt.data()), salt.size()));
}

}  // namespace chromeos
