// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Invoke;
using ::testing::WithArgs;
using ::testing::_;

namespace chromeos {

class MockCryptohomeLibrary : public CryptohomeLibrary {
 public:
  MockCryptohomeLibrary() : outcome_(false), code_(0) {
  }
  virtual ~MockCryptohomeLibrary() {}
  void SetUp(bool outcome, int code) {
    outcome_ = outcome;
    code_ = code;
    ON_CALL(*this, AsyncCheckKey(_, _, _))
        .WillByDefault(
            WithArgs<2>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
    ON_CALL(*this, AsyncMigrateKey(_, _, _, _))
        .WillByDefault(
            WithArgs<3>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
    ON_CALL(*this, AsyncMount(_, _, _, _))
        .WillByDefault(
            WithArgs<3>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
    ON_CALL(*this, AsyncMountForBwsi(_))
        .WillByDefault(
            WithArgs<0>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
    ON_CALL(*this, AsyncRemove(_, _))
        .WillByDefault(
            WithArgs<1>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
    ON_CALL(*this, AsyncDoAutomaticFreeDiskSpaceControl(_))
        .WillByDefault(
            WithArgs<0>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
  }
  MOCK_METHOD2(CheckKey, bool(const std::string& user_email,
                              const std::string& passhash));
  MOCK_METHOD3(AsyncCheckKey, bool(const std::string& user_email,
                                   const std::string& passhash,
                                   Delegate* callback));
  MOCK_METHOD3(MigrateKey, bool(const std::string& user_email,
                                const std::string& old_hash,
                                const std::string& new_hash));
  MOCK_METHOD4(AsyncMigrateKey, bool(const std::string& user_email,
                                     const std::string& old_hash,
                                     const std::string& new_hash,
                                     Delegate* callback));
  MOCK_METHOD3(Mount, bool(const std::string& user_email,
                           const std::string& passhash,
                           int* error_code));
  MOCK_METHOD4(AsyncMount, bool(const std::string& user_email,
                                const std::string& passhash,
                                const bool create_if_missing,
                                Delegate* callback));
  MOCK_METHOD1(MountForBwsi, bool(int*));
  MOCK_METHOD1(AsyncMountForBwsi, bool(Delegate* callback));
  MOCK_METHOD0(Unmount, bool(void));
  MOCK_METHOD1(Remove, bool(const std::string& user_email));
  MOCK_METHOD2(AsyncRemove, bool(const std::string& user_email, Delegate* d));
  MOCK_METHOD0(IsMounted, bool(void));
  MOCK_METHOD0(GetSystemSalt, CryptohomeBlob(void));
  MOCK_METHOD1(AsyncDoAutomaticFreeDiskSpaceControl, bool(Delegate* callback));

  MOCK_METHOD0(TpmIsReady, bool(void));
  MOCK_METHOD0(TpmIsEnabled, bool(void));
  MOCK_METHOD0(TpmIsOwned, bool(void));
  MOCK_METHOD0(TpmIsBeingOwned, bool(void));
  MOCK_METHOD1(TpmGetPassword, bool(std::string* password));
  MOCK_METHOD0(TpmCanAttemptOwnership, void(void));
  MOCK_METHOD0(TpmClearStoredPassword, void(void));
  MOCK_METHOD0(Pkcs11IsTpmTokenReady, bool(void));
  MOCK_METHOD2(Pkcs11GetTpmTokenInfo, void(std::string*, std::string*));

  MOCK_METHOD2(InstallAttributesGet, bool(const std::string&, std::string*));
  MOCK_METHOD2(InstallAttributesSet, bool(const std::string&,
                                          const std::string&));
  MOCK_METHOD0(InstallAttributesCount, int(void));
  MOCK_METHOD0(InstallAttributesFinalize, bool(void));
  MOCK_METHOD0(InstallAttributesIsReady, bool(void));
  MOCK_METHOD0(InstallAttributesIsSecure, bool(void));
  MOCK_METHOD0(InstallAttributesIsInvalid, bool(void));
  MOCK_METHOD0(InstallAttributesIsFirstInstall, bool(void));

  void SetAsyncBehavior(bool outcome, int code) {
    outcome_ = outcome;
    code_ = code;
  }

  bool DoCallback(Delegate* d) {
    d->OnComplete(outcome_, code_);
    return true;
  }

 private:
  bool outcome_;
  int code_;
  DISALLOW_COPY_AND_ASSIGN(MockCryptohomeLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
