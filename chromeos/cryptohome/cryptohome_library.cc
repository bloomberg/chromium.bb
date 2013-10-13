// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_library.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
namespace {

CryptohomeLibrary* g_cryptohome_library = NULL;

}  // namespace

CryptohomeLibrary::CryptohomeLibrary() {
}

CryptohomeLibrary::~CryptohomeLibrary() {
}

void CryptohomeLibrary::GetSystemSalt(
    const GetSystemSaltCallback& callback) {
  // TODO(hashimoto): Stop using GetSystemSaltSynt(). crbug.com/141009
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, GetSystemSaltSync()));
}

std::string CryptohomeLibrary::GetSystemSaltSync() {
  LoadSystemSalt();  // no-op if it's already loaded.
  return system_salt_;
}

std::string CryptohomeLibrary::GetCachedSystemSalt() {
  return system_salt_;
}

void CryptohomeLibrary::LoadSystemSalt() {
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
void CryptohomeLibrary::Initialize() {
  CHECK(!g_cryptohome_library);
  g_cryptohome_library = new CryptohomeLibrary();
}

// static
bool CryptohomeLibrary::IsInitialized() {
  return g_cryptohome_library;
}

// static
void CryptohomeLibrary::Shutdown() {
  CHECK(g_cryptohome_library);
  delete g_cryptohome_library;
  g_cryptohome_library = NULL;
}

// static
CryptohomeLibrary* CryptohomeLibrary::Get() {
  CHECK(g_cryptohome_library)
      << "CryptohomeLibrary::Get() called before Initialize()";
  return g_cryptohome_library;
}

// static
std::string CryptohomeLibrary::ConvertRawSaltToHexString(
    const std::vector<uint8>& salt) {
  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(salt.data()), salt.size()));
}

}  // namespace chromeos
