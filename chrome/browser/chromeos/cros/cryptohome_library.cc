// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS cryptohome library APIs.
class CryptohomeLibraryImpl : public CryptohomeLibrary {
 public:
  CryptohomeLibraryImpl() {
    if (CrosLibrary::Get()->EnsureLoaded())
      Init();
  }
  virtual ~CryptohomeLibraryImpl() {}

  bool CheckKey(const std::string& user_email, const std::string& passhash) {
    return chromeos::CryptohomeCheckKey(user_email.c_str(), passhash.c_str());
  }

  bool AsyncCheckKey(const std::string& user_email,
                     const std::string& passhash,
                     Delegate* d) {
    return CacheCallback(
        chromeos::CryptohomeAsyncCheckKey(user_email.c_str(), passhash.c_str()),
        d,
        "Couldn't initiate async check of user's key.");
  }

  bool MigrateKey(const std::string& user_email,
                  const std::string& old_hash,
                  const std::string& new_hash) {
    return chromeos::CryptohomeMigrateKey(user_email.c_str(),
                                          old_hash.c_str(),
                                          new_hash.c_str());
  }

  bool AsyncMigrateKey(const std::string& user_email,
                       const std::string& old_hash,
                       const std::string& new_hash,
                       Delegate* d) {
    return CacheCallback(
        chromeos::CryptohomeAsyncMigrateKey(user_email.c_str(),
                                            old_hash.c_str(),
                                            new_hash.c_str()),
        d,
        "Couldn't initiate aync migration of user's key");
  }

  bool Mount(const std::string& user_email,
             const std::string& passhash,
             int* error_code) {
    return chromeos::CryptohomeMountAllowFail(user_email.c_str(),
                                              passhash.c_str(),
                                              error_code);
  }

  bool AsyncMount(const std::string& user_email,
                  const std::string& passhash,
                  const bool create_if_missing,
                  Delegate* d) {
    return CacheCallback(
        chromeos::CryptohomeAsyncMountSafe(user_email.c_str(),
                                           passhash.c_str(),
                                           create_if_missing,
                                           false,
                                           NULL),
        d,
        "Couldn't initiate async mount of cryptohome.");
  }

  bool MountForBwsi(int* error_code) {
    return chromeos::CryptohomeMountGuest(error_code);
  }

  bool AsyncMountForBwsi(Delegate* d) {
    return CacheCallback(chromeos::CryptohomeAsyncMountGuest(),
                         d,
                         "Couldn't initiate async mount of cryptohome.");
  }

  bool Unmount() {
    return chromeos::CryptohomeUnmount();
  }

  bool Remove(const std::string& user_email) {
    return chromeos::CryptohomeRemove(user_email.c_str());
  }

  bool AsyncRemove(const std::string& user_email, Delegate* d) {
    return CacheCallback(
        chromeos::CryptohomeAsyncRemove(user_email.c_str()),
        d,
        "Couldn't initiate async removal of cryptohome.");
  }

  bool IsMounted() {
    return chromeos::CryptohomeIsMounted();
  }

  CryptohomeBlob GetSystemSalt() {
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

  bool AsyncDoAutomaticFreeDiskSpaceControl(Delegate* d) {
    return CacheCallback(
        chromeos::CryptohomeAsyncDoAutomaticFreeDiskSpaceControl(),
        d,
        "Couldn't do automatic free disk space control.");
  }

  bool TpmIsReady() {
    return chromeos::CryptohomeTpmIsReady();
  }

  bool TpmIsEnabled() {
    return chromeos::CryptohomeTpmIsEnabled();
  }

  bool TpmIsOwned() {
    return chromeos::CryptohomeTpmIsOwned();
  }

  bool TpmIsBeingOwned() {
    return chromeos::CryptohomeTpmIsBeingOwned();
  }

  bool TpmGetPassword(std::string* password) {
    char *password_buf;
    bool result = chromeos::CryptohomeTpmGetPasswordSafe(&password_buf);
    *password = password_buf;
    chromeos::CryptohomeFreeString(password_buf);
    return result;
  }

  void TpmCanAttemptOwnership() {
    chromeos::CryptohomeTpmCanAttemptOwnership();
  }

  void TpmClearStoredPassword() {
    chromeos::CryptohomeTpmClearStoredPassword();
  }

  bool InstallAttributesGet(const std::string& name, std::string* value) {
    char* local_value;
    bool done =
        chromeos::CryptohomeInstallAttributesGet(name.c_str(), &local_value);
    if (done) {
      *value = local_value;
      chromeos::CryptohomeFreeString(local_value);
    }
    return done;
  }

  bool InstallAttributesSet(const std::string& name, const std::string& value) {
    return chromeos::CryptohomeInstallAttributesSet(name.c_str(),
                                                    value.c_str());
  }

  int InstallAttributesCount() {
    return chromeos::CryptohomeInstallAttributesCount();
  }

  bool InstallAttributesFinalize() {
    return chromeos::CryptohomeInstallAttributesFinalize();
  }

  bool InstallAttributesIsReady() {
    return chromeos::CryptohomeInstallAttributesIsReady();
  }

  bool InstallAttributesIsSecure() {
    return chromeos::CryptohomeInstallAttributesIsSecure();
  }

  bool InstallAttributesIsInvalid() {
    return chromeos::CryptohomeInstallAttributesIsInvalid();
  }

  bool InstallAttributesIsFirstInstall() {
    return chromeos::CryptohomeInstallAttributesIsFirstInstall();
  }

  void Pkcs11GetTpmTokenInfo(std::string* label, std::string* user_pin) {
    chromeos::CryptohomePkcs11GetTpmTokenInfo(label, user_pin);
  }

  bool Pkcs11IsTpmTokenReady() {
    return chromeos::CryptohomePkcs11IsTpmTokenReady();
  }

 private:
  static void Handler(const chromeos::CryptohomeAsyncCallStatus& event,
                      void* cryptohome_library) {
    CryptohomeLibraryImpl* library =
        reinterpret_cast<CryptohomeLibraryImpl*>(cryptohome_library);
    library->Dispatch(event);
  }

  void Init() {
    cryptohome_connection_ = chromeos::CryptohomeMonitorSession(&Handler, this);
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

  bool CheckKey(const std::string& user_email, const std::string& passhash) {
    return true;
  }

  bool AsyncCheckKey(const std::string& user_email,
                     const std::string& passhash,
                     Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }

  bool MigrateKey(const std::string& user_email,
                  const std::string& old_hash,
                  const std::string& new_hash) {
    return true;
  }

  bool AsyncMigrateKey(const std::string& user_email,
                       const std::string& old_hash,
                       const std::string& new_hash,
                       Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }

  bool Mount(const std::string& user_email,
             const std::string& passhash,
             int* error_code) {
    // For testing password change.
    if (user_email ==
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kLoginUserWithNewPassword)) {
      *error_code = kCryptohomeMountErrorKeyFailure;
      return false;
    }

    return true;
  }

  bool AsyncMount(const std::string& user_email,
                  const std::string& passhash,
                  const bool create_if_missing,
                  Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }

  bool MountForBwsi(int* error_code) {
    return true;
  }

  bool AsyncMountForBwsi(Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }

  bool Unmount() {
    return true;
  }

  bool Remove(const std::string& user_email) {
    return true;
  }

  bool AsyncRemove(const std::string& user_email, Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }

  bool IsMounted() {
    return true;
  }

  CryptohomeBlob GetSystemSalt() {
    CryptohomeBlob salt = CryptohomeBlob();
    salt.push_back(0);
    salt.push_back(0);
    return salt;
  }

  bool AsyncDoAutomaticFreeDiskSpaceControl(Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }

  // Tpm begin ready after 20-th call.
  bool TpmIsReady() {
    static int counter = 0;
    return ++counter > 20;
  }

  bool TpmIsEnabled() {
    return true;
  }

  bool TpmIsOwned() {
    return true;
  }

  bool TpmIsBeingOwned() {
    return true;
  }

  bool TpmGetPassword(std::string* password) {
    *password = "Stub-TPM-password";
    return true;
  }

  void TpmCanAttemptOwnership() {}

  void TpmClearStoredPassword() {}

  bool InstallAttributesGet(const std::string& name, std::string* value) {
    if (install_attrs_.find(name) != install_attrs_.end()) {
      *value = install_attrs_[name];
      return true;
    }
    return false;
  }

  bool InstallAttributesSet(const std::string& name, const std::string& value) {
    install_attrs_[name] = value;
    return true;
  }

  int InstallAttributesCount() {
    return install_attrs_.size();
  }

  bool InstallAttributesFinalize() {
    locked_ = true;
    return true;
  }

  bool InstallAttributesIsReady() {
    return true;
  }

  bool InstallAttributesIsSecure() {
    return false;
  }

  bool InstallAttributesIsInvalid() {
    return false;
  }

  bool InstallAttributesIsFirstInstall() {
    return !locked_;
  }

  void Pkcs11GetTpmTokenInfo(std::string* label,
                             std::string* user_pin) {
    *label = "Stub TPM Token";
    *user_pin = "012345";
  }

  bool Pkcs11IsTpmTokenReady() { return true; }

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
  if (stub)
    return new CryptohomeLibraryStubImpl();
  else
    return new CryptohomeLibraryImpl();
}

} // namespace chromeos
