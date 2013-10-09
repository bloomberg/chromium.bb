// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_library.h"

#include <map>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {

const char kStubSystemSalt[] = "stub_system_salt";

}  // namespace

// This class handles the interaction with the ChromeOS cryptohome library APIs.
class CryptohomeLibraryImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryImpl() {
  }

  virtual ~CryptohomeLibraryImpl() {
  }

  virtual std::string GetSystemSalt() OVERRIDE {
    LoadSystemSalt();  // no-op if it's already loaded.
    return system_salt_;
  }

  virtual std::string GetCachedSystemSalt() OVERRIDE {
    return system_salt_;
  }

 private:
  void LoadSystemSalt() {
    if (!system_salt_.empty())
      return;
    std::vector<uint8> salt;
    DBusThreadManager::Get()->GetCryptohomeClient()->GetSystemSalt(&salt);
    if (salt.empty() || salt.size() % 2 != 0U) {
      LOG(WARNING) << "System salt not available";
      return;
    }
    system_salt_ = StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(salt.data()), salt.size()));
  }

  std::string system_salt_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryImpl);
};

class CryptohomeLibraryStubImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryStubImpl() {}
  virtual ~CryptohomeLibraryStubImpl() {}

  virtual std::string GetSystemSalt() OVERRIDE {
    return kStubSystemSalt;
  }

  virtual std::string GetCachedSystemSalt() OVERRIDE {
    return kStubSystemSalt;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryStubImpl);
};

CryptohomeLibrary::CryptohomeLibrary() {}
CryptohomeLibrary::~CryptohomeLibrary() {}

static CryptohomeLibrary* g_cryptohome_library = NULL;
static CryptohomeLibrary* g_test_cryptohome_library = NULL;

// static
void CryptohomeLibrary::Initialize() {
  CHECK(!g_cryptohome_library);
  if (base::SysInfo::IsRunningOnChromeOS())
    g_cryptohome_library = new CryptohomeLibraryImpl();
  else
    g_cryptohome_library = new CryptohomeLibraryStubImpl();
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
  CHECK(g_cryptohome_library || g_test_cryptohome_library)
      << "CryptohomeLibrary::Get() called before Initialize()";
  if (g_test_cryptohome_library)
    return g_test_cryptohome_library;
  return g_cryptohome_library;
}

// static
void CryptohomeLibrary::SetForTest(CryptohomeLibrary* impl) {
  CHECK(!g_test_cryptohome_library || !impl);
  g_test_cryptohome_library = impl;
}

// static
CryptohomeLibrary* CryptohomeLibrary::GetTestImpl() {
  return new CryptohomeLibraryStubImpl();
}

}  // namespace chromeos
