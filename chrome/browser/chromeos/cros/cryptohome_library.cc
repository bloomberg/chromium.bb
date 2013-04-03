// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include <map>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {

const char kStubSystemSalt[] = "stub_system_salt";

// Does nothing.  Used as a Cryptohome::VoidMethodCallback.
void DoNothing(DBusMethodCallStatus call_status) {}

}  // namespace

// This class handles the interaction with the ChromeOS cryptohome library APIs.
class CryptohomeLibraryImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryImpl() : weak_ptr_factory_(this) {
  }

  virtual ~CryptohomeLibraryImpl() {
  }

  virtual bool TpmIsEnabled() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->CallTpmIsEnabledAndBlock(
        &result);
    return result;
  }

  virtual bool TpmIsOwned() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->CallTpmIsOwnedAndBlock(
        &result);
    return result;
  }

  virtual bool TpmIsBeingOwned() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        CallTpmIsBeingOwnedAndBlock(&result);
    return result;
  }

  virtual void TpmCanAttemptOwnership() OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership(
        base::Bind(&DoNothing));
  }

  virtual void TpmClearStoredPassword() OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        CallTpmClearStoredPasswordAndBlock();
  }

  virtual bool InstallAttributesGet(
      const std::string& name, std::string* value) OVERRIDE {
    std::vector<uint8> buf;
    bool success = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesGet(name, &buf, &success);
    if (success) {
      // Cryptohome returns 'buf' with a terminating '\0' character.
      DCHECK(!buf.empty());
      DCHECK_EQ(buf.back(), 0);
      value->assign(reinterpret_cast<char*>(buf.data()), buf.size() - 1);
    }
    return success;
  }

  virtual bool InstallAttributesSet(
      const std::string& name, const std::string& value) OVERRIDE {
    std::vector<uint8> buf(value.c_str(), value.c_str() + value.size() + 1);
    bool success = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesSet(name, buf, &success);
    return success;
  }

  virtual bool InstallAttributesFinalize() OVERRIDE {
    bool success = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesFinalize(&success);
    return success;
  }

  virtual bool InstallAttributesIsInvalid() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesIsInvalid(&result);
    return result;
  }

  virtual bool InstallAttributesIsFirstInstall() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesIsFirstInstall(&result);
    return result;
  }

  virtual std::string GetSystemSalt() OVERRIDE {
    LoadSystemSalt();  // no-op if it's already loaded.
    return StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(system_salt_.data()),
        system_salt_.size()));
  }

 private:
  void LoadSystemSalt() {
    if (!system_salt_.empty())
      return;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        GetSystemSalt(&system_salt_);
    CHECK(!system_salt_.empty());
    CHECK_EQ(system_salt_.size() % 2, 0U);
  }

  base::WeakPtrFactory<CryptohomeLibraryImpl> weak_ptr_factory_;
  std::vector<uint8> system_salt_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryImpl);
};

class CryptohomeLibraryStubImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryStubImpl()
    : locked_(false) {}
  virtual ~CryptohomeLibraryStubImpl() {}

  virtual bool TpmIsEnabled() OVERRIDE {
    return true;
  }

  virtual bool TpmIsOwned() OVERRIDE {
    return true;
  }

  virtual bool TpmIsBeingOwned() OVERRIDE {
    return true;
  }

  virtual void TpmCanAttemptOwnership() OVERRIDE {}

  virtual void TpmClearStoredPassword() OVERRIDE {}

  virtual bool InstallAttributesGet(
      const std::string& name, std::string* value) OVERRIDE {
    if (install_attrs_.find(name) != install_attrs_.end()) {
      *value = install_attrs_[name];
      return true;
    }
    return false;
  }

  virtual bool InstallAttributesSet(
      const std::string& name, const std::string& value) OVERRIDE {
    install_attrs_[name] = value;
    return true;
  }

  virtual bool InstallAttributesFinalize() OVERRIDE {
    locked_ = true;
    return true;
  }

  virtual bool InstallAttributesIsInvalid() OVERRIDE {
    return false;
  }

  virtual bool InstallAttributesIsFirstInstall() OVERRIDE {
    return !locked_;
  }

  virtual std::string GetSystemSalt() OVERRIDE {
    return kStubSystemSalt;
  }

 private:
  std::map<std::string, std::string> install_attrs_;
  bool locked_;
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryStubImpl);
};

CryptohomeLibrary::CryptohomeLibrary() {}
CryptohomeLibrary::~CryptohomeLibrary() {}

// static
CryptohomeLibrary* CryptohomeLibrary::GetImpl(bool stub) {
  CryptohomeLibrary* impl;
  if (stub)
    impl = new CryptohomeLibraryStubImpl();
  else
    impl = new CryptohomeLibraryImpl();
  return impl;
}

} // namespace chromeos
