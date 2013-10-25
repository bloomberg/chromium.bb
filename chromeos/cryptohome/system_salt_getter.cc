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
  DBusThreadManager::Get()->GetCryptohomeClient()->WaitForServiceToBeAvailable(
      base::Bind(&SystemSaltGetter::GetSystemSaltInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

std::string SystemSaltGetter::GetSystemSaltSync() {
  LoadSystemSalt();  // no-op if it's already loaded.
  return system_salt_;
}

void SystemSaltGetter::GetSystemSaltInternal(
    const GetSystemSaltCallback& callback,
    bool service_is_available) {
  LOG_IF(ERROR, !service_is_available) << "WaitForServiceToBeAvailable failed.";
  // TODO(hashimoto): Stop using GetSystemSaltSync(). crbug.com/141009
  callback.Run(GetSystemSaltSync());
}

void SystemSaltGetter::LoadSystemSalt() {
  if (!system_salt_.empty())
    return;
  std::vector<uint8> salt;
  DBusThreadManager::Get()->GetCryptohomeClient()->GetSystemSalt(&salt);
  if (salt.empty() || salt.size() % 2 != 0U) {
    LOG(WARNING) << "System salt not available";
    return;
  }
  system_salt_ = ConvertRawSaltToHexString(salt);
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
