// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/dbus/cryptohome_client.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/encryptor.h"
#include "crypto/sha2.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace {

const char kStubSystemSalt[] = "stub_system_salt";
const int kPassHashLen = 32;

}

namespace chromeos {

// This class handles the interaction with the ChromeOS cryptohome library APIs.
class CryptohomeLibraryImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryImpl() : weak_ptr_factory_(this) {
    DBusThreadManager::Get()->GetCryptohomeClient()->SetAsyncCallStatusHandler(
        base::Bind(&CryptohomeLibraryImpl::HandleAsyncResponse,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~CryptohomeLibraryImpl() {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        ResetAsyncCallStatusHandler();
  }

  virtual void AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             AsyncMethodCallback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncCheckKey(user_email, passhash, base::Bind(
            &CryptohomeLibraryImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async check of user's key."));
  }

  virtual void AsyncMigrateKey(const std::string& user_email,
                       const std::string& old_hash,
                       const std::string& new_hash,
                       AsyncMethodCallback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncMigrateKey(user_email, old_hash, new_hash, base::Bind(
            &CryptohomeLibraryImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate aync migration of user's key"));
  }

  virtual void AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          const bool create_if_missing,
                          AsyncMethodCallback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncMount(user_email, passhash, create_if_missing, base::Bind(
            &CryptohomeLibraryImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async mount of cryptohome."));
  }

  virtual void AsyncMountGuest(AsyncMethodCallback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncMountGuest(base::Bind(
            &CryptohomeLibraryImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async mount of cryptohome."));
  }

  virtual void AsyncRemove(
      const std::string& user_email, AsyncMethodCallback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncRemove(user_email, base::Bind(
            &CryptohomeLibraryImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async removal of cryptohome."));
  }

  virtual bool IsMounted() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->IsMounted(&result);
    return result;
  }

  virtual bool TpmIsReady() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsReady(&result);
    return result;
  }

  virtual bool TpmIsEnabled() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsEnabled(&result);
    return result;
  }

  virtual bool TpmIsOwned() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsOwned(&result);
    return result;
  }

  virtual bool TpmIsBeingOwned() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsBeingOwned(&result);
    return result;
  }

  virtual bool TpmGetPassword(std::string* password) OVERRIDE {
    return DBusThreadManager::Get()->GetCryptohomeClient()->
        TpmGetPassword(password);
  }

  virtual void TpmCanAttemptOwnership() OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership();
  }

  virtual void TpmClearStoredPassword() OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmClearStoredPassword();
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

  virtual bool InstallAttributesIsReady() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        InstallAttributesIsReady(&result);
    return result;
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

  virtual void Pkcs11GetTpmTokenInfo(
      std::string* label, std::string* user_pin) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11GetTpmTokenInfo(
        label, user_pin);
  }

  virtual bool Pkcs11IsTpmTokenReady() OVERRIDE {
    bool result = false;
    DBusThreadManager::Get()->GetCryptohomeClient()->
        Pkcs11IsTpmTokenReady(&result);
    return result;
  }

  virtual std::string HashPassword(const std::string& password) OVERRIDE {
    // Get salt, ascii encode, update sha with that, then update with ascii
    // of password, then end.
    std::string ascii_salt = GetSystemSalt();
    char passhash_buf[kPassHashLen];

    // Hash salt and password
    crypto::SHA256HashString(ascii_salt + password,
                             &passhash_buf, sizeof(passhash_buf));

    return StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(passhash_buf),
        sizeof(passhash_buf) / 2));
  }

  virtual std::string GetSystemSalt() OVERRIDE {
    LoadSystemSalt();  // no-op if it's already loaded.
    return StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(system_salt_.data()),
        system_salt_.size()));
  }

 private:
  typedef base::hash_map<int, AsyncMethodCallback> CallbackMap;

  // Hanldes the response for async calls.
  // Below is described how async calls work.
  // 1. CryptohomeClient::AsyncXXX returns "async ID".
  // 2. RegisterAsyncCallback registers the "async ID" with the user-provided
  //    callback.
  // 3. Cryptohome will return the result asynchronously as a signal with
  //    "async ID"
  // 4. "HandleAsyncResponse" handles the result signal and call the registered
  //    callback associated with the "async ID".
  void HandleAsyncResponse(int async_id, bool return_status, int return_code) {
    const CallbackMap::iterator it = callback_map_.find(async_id);
    if (it == callback_map_.end()) {
      LOG(ERROR) << "Received signal for unknown async_id " << async_id;
      return;
    }
    it->second.Run(return_status, return_code);
    callback_map_.erase(it);
  }

  // Registers a callback which is called when the result for AsyncXXX is ready.
  void RegisterAsyncCallback(AsyncMethodCallback callback,
                             const char* error,
                             int async_id) {
    if (async_id == 0) {
      LOG(ERROR) << error;
      return;
    }
    VLOG(1) << "Adding handler for " << async_id;
    DCHECK_EQ(callback_map_.count(async_id), 0U);
    callback_map_[async_id] = callback;
  }

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
  mutable CallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryImpl);
};

class CryptohomeLibraryStubImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryStubImpl()
    : locked_(false) {}
  virtual ~CryptohomeLibraryStubImpl() {}

  virtual void AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             AsyncMethodCallback callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
  }

  virtual void AsyncMigrateKey(const std::string& user_email,
                               const std::string& old_hash,
                               const std::string& new_hash,
                               AsyncMethodCallback callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
  }

  virtual void AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          const bool create_if_missing,
                          AsyncMethodCallback callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
  }

  virtual void AsyncMountGuest(AsyncMethodCallback callback) OVERRIDE {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&DoStubCallback, callback));
  }

  virtual void AsyncRemove(
      const std::string& user_email, AsyncMethodCallback callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
  }

  virtual bool IsMounted() OVERRIDE {
    return true;
  }

  // Tpm begin ready after 20-th call.
  virtual bool TpmIsReady() OVERRIDE {
    static int counter = 0;
    return ++counter > 20;
  }

  virtual bool TpmIsEnabled() OVERRIDE {
    return true;
  }

  virtual bool TpmIsOwned() OVERRIDE {
    return true;
  }

  virtual bool TpmIsBeingOwned() OVERRIDE {
    return true;
  }

  virtual bool TpmGetPassword(std::string* password) OVERRIDE {
    *password = "Stub-TPM-password";
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

  virtual bool InstallAttributesIsReady() OVERRIDE {
    return true;
  }

  virtual bool InstallAttributesIsInvalid() OVERRIDE {
    return false;
  }

  virtual bool InstallAttributesIsFirstInstall() OVERRIDE {
    return !locked_;
  }

  virtual void Pkcs11GetTpmTokenInfo(std::string* label,
                                     std::string* user_pin) OVERRIDE {
    *label = "Stub TPM Token";
    *user_pin = "012345";
  }

  virtual bool Pkcs11IsTpmTokenReady() OVERRIDE { return true; }

  virtual std::string HashPassword(const std::string& password) OVERRIDE {
    return StringToLowerASCII(base::HexEncode(
            reinterpret_cast<const void*>(password.data()),
            password.length()));
  }

  virtual std::string GetSystemSalt() OVERRIDE {
    return kStubSystemSalt;
  }

 private:
  static void DoStubCallback(AsyncMethodCallback callback) {
    callback.Run(true, cryptohome::MOUNT_ERROR_NONE);
  }

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
