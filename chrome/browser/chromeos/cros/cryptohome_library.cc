// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
        chromeos::CryptohomeAsyncMount(user_email.c_str(),
                                       passhash.c_str(),
                                       create_if_missing,
                                       "",
                                       std::vector<std::string>()),
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
    return chromeos::CryptohomeGetSystemSalt();
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
    return chromeos::CryptohomeTpmGetPassword(password);
  }

  void TpmCanAttemptOwnership() {
    chromeos::CryptohomeTpmCanAttemptOwnership();
  }

  void TpmClearStoredPassword() {
    chromeos::CryptohomeTpmClearStoredPassword();
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
    if (!callback_map_[event.async_id]) {
      LOG(ERROR) << "Received signal for unknown async_id " << event.async_id;
      return;
    }
    callback_map_[event.async_id]->OnComplete(event.return_status,
                                              event.return_code);
    callback_map_[event.async_id] = NULL;
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
  CryptohomeLibraryStubImpl() {}
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

 private:
  static void DoStubCallback(Delegate* callback) {
    callback->OnComplete(true, kCryptohomeMountErrorNone);
  }
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryStubImpl);
};

// static
CryptohomeLibrary* CryptohomeLibrary::GetImpl(bool stub) {
  if (stub)
    return new CryptohomeLibraryStubImpl();
  else
    return new CryptohomeLibraryImpl();
}

} // namespace chromeos
