// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {
  const char kStubSystemSalt[] = "stub_system_salt";
}

namespace chromeos {

// This class handles the interaction with the ChromeOS cryptohome library APIs.
class CryptohomeLibraryImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryImpl() {}
  virtual ~CryptohomeLibraryImpl() {}

  virtual void Init() OVERRIDE {
    DCHECK(CrosLibrary::Get()->libcros_loaded());
    cryptohome_connection_ = chromeos::CryptohomeMonitorSession(&Handler, this);
  }

  virtual bool AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             Delegate* d) OVERRIDE {
    return CacheCallback(
        chromeos::CryptohomeAsyncCheckKey(user_email.c_str(), passhash.c_str()),
        d,
        "Couldn't initiate async check of user's key.");
  }

  virtual bool AsyncMigrateKey(const std::string& user_email,
                       const std::string& old_hash,
                       const std::string& new_hash,
                       Delegate* d) OVERRIDE {
    return CacheCallback(
        chromeos::CryptohomeAsyncMigrateKey(user_email.c_str(),
                                            old_hash.c_str(),
                                            new_hash.c_str()),
        d,
        "Couldn't initiate aync migration of user's key");
  }

  virtual bool AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          const bool create_if_missing,
                          Delegate* d) OVERRIDE {
    return CacheCallback(
        chromeos::CryptohomeAsyncMountSafe(user_email.c_str(),
                                           passhash.c_str(),
                                           create_if_missing,
                                           false,
                                           NULL),
        d,
        "Couldn't initiate async mount of cryptohome.");
  }

  virtual bool AsyncMountForBwsi(Delegate* d) OVERRIDE {
    return CacheCallback(chromeos::CryptohomeAsyncMountGuest(),
                         d,
                         "Couldn't initiate async mount of cryptohome.");
  }

  virtual bool AsyncRemove(
      const std::string& user_email, Delegate* d) OVERRIDE {
    return CacheCallback(
        chromeos::CryptohomeAsyncRemove(user_email.c_str()),
        d,
        "Couldn't initiate async removal of cryptohome.");
  }

  virtual bool IsMounted() OVERRIDE {
    return chromeos::CryptohomeIsMounted();
  }

  virtual CryptohomeBlob GetSystemSalt() OVERRIDE {
    CryptohomeBlob system_salt;
    char* salt_buf;
    int salt_len;
    bool result = chromeos::CryptohomeGetSystemSaltSafe(&salt_buf, &salt_len);
    if (result) {
      system_salt.resize(salt_len);
      if ((int)system_salt.size() == salt_len) {
        memcpy(&system_salt[0], static_cast<const void*>(salt_buf),
               salt_len);
      } else {
        system_salt.clear();
      }
    }
    return system_salt;
  }

  virtual bool AsyncSetOwnerUser(
      const std::string& username, Delegate* d) OVERRIDE {
    return CacheCallback(
        chromeos::CryptohomeAsyncSetOwnerUser(username.c_str()),
        d,
        "Couldn't do set owner user in Cryptohomed.");
  }

  virtual bool TpmIsReady() OVERRIDE {
    return chromeos::CryptohomeTpmIsReady();
  }

  virtual bool TpmIsEnabled() OVERRIDE {
    return chromeos::CryptohomeTpmIsEnabled();
  }

  virtual bool TpmIsOwned() OVERRIDE {
    return chromeos::CryptohomeTpmIsOwned();
  }

  virtual bool TpmIsBeingOwned() OVERRIDE {
    return chromeos::CryptohomeTpmIsBeingOwned();
  }

  virtual bool TpmGetPassword(std::string* password) OVERRIDE {
    char *password_buf;
    bool result = chromeos::CryptohomeTpmGetPasswordSafe(&password_buf);
    *password = password_buf;
    chromeos::CryptohomeFreeString(password_buf);
    return result;
  }

  virtual void TpmCanAttemptOwnership() OVERRIDE {
    chromeos::CryptohomeTpmCanAttemptOwnership();
  }

  virtual void TpmClearStoredPassword() OVERRIDE {
    chromeos::CryptohomeTpmClearStoredPassword();
  }

  virtual bool InstallAttributesGet(
      const std::string& name, std::string* value) OVERRIDE {
    char* local_value;
    bool done =
        chromeos::CryptohomeInstallAttributesGet(name.c_str(), &local_value);
    if (done) {
      *value = local_value;
      chromeos::CryptohomeFreeString(local_value);
    }
    return done;
  }

  virtual bool InstallAttributesSet(
      const std::string& name, const std::string& value) OVERRIDE {
    return chromeos::CryptohomeInstallAttributesSet(name.c_str(),
                                                    value.c_str());
  }

  virtual bool InstallAttributesFinalize() OVERRIDE {
    return chromeos::CryptohomeInstallAttributesFinalize();
  }

  virtual bool InstallAttributesIsReady() OVERRIDE {
    return chromeos::CryptohomeInstallAttributesIsReady();
  }

  virtual bool InstallAttributesIsInvalid() OVERRIDE {
    return chromeos::CryptohomeInstallAttributesIsInvalid();
  }

  virtual bool InstallAttributesIsFirstInstall() OVERRIDE {
    return chromeos::CryptohomeInstallAttributesIsFirstInstall();
  }

  virtual void Pkcs11GetTpmTokenInfo(
      std::string* label, std::string* user_pin) OVERRIDE {
    chromeos::CryptohomePkcs11GetTpmTokenInfo(label, user_pin);
  }

  virtual bool Pkcs11IsTpmTokenReady() OVERRIDE {
    return chromeos::CryptohomePkcs11IsTpmTokenReady();
  }

 private:
  static void Handler(const chromeos::CryptohomeAsyncCallStatus& event,
                      void* cryptohome_library) {
    CryptohomeLibraryImpl* library =
        reinterpret_cast<CryptohomeLibraryImpl*>(cryptohome_library);
    library->Dispatch(event);
  }

  void Dispatch(const chromeos::CryptohomeAsyncCallStatus& event) {
    const CallbackMap::iterator callback = callback_map_.find(event.async_id);
    if (callback == callback_map_.end()) {
      LOG(ERROR) << "Received signal for unknown async_id " << event.async_id;
      return;
    }
    if (callback->second)
      callback->second->OnComplete(event.return_status, event.return_code);
    callback_map_.erase(callback);
  }

  bool CacheCallback(int async_id, Delegate* d, const char* error) {
    if (async_id == 0) {
      LOG(ERROR) << error;
      return false;
    }
    VLOG(1) << "Adding handler for " << async_id;
    callback_map_[async_id] = d;
    return true;
  }

  typedef base::hash_map<int, Delegate*> CallbackMap;
  mutable CallbackMap callback_map_;

  void* cryptohome_connection_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryImpl);
};

class CryptohomeLibraryStubImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryStubImpl()
    : locked_(false) {}
  virtual ~CryptohomeLibraryStubImpl() {}

  virtual void Init() OVERRIDE {}

  virtual bool AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             Delegate* callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
    return true;
  }

  virtual bool AsyncMigrateKey(const std::string& user_email,
                               const std::string& old_hash,
                               const std::string& new_hash,
                               Delegate* callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
    return true;
  }

  virtual bool AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          const bool create_if_missing,
                          Delegate* callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
    return true;
  }

  virtual bool AsyncMountForBwsi(Delegate* callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
    return true;
  }

  virtual bool AsyncRemove(
      const std::string& user_email, Delegate* callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
    return true;
  }

  virtual bool IsMounted() OVERRIDE {
    return true;
  }

  virtual CryptohomeBlob GetSystemSalt() OVERRIDE {
    CryptohomeBlob salt = CryptohomeBlob();
    for (size_t i = 0; i < strlen(kStubSystemSalt); i++)
      salt.push_back(static_cast<unsigned char>(kStubSystemSalt[i]));

    return salt;
  }

  virtual bool AsyncSetOwnerUser(
      const std::string& username, Delegate* callback) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DoStubCallback, callback));
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

 private:
  static void DoStubCallback(Delegate* callback) {
    if (callback)
      callback->OnComplete(true, kCryptohomeMountErrorNone);
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
  impl->Init();
  return impl;
}

} // namespace chromeos
