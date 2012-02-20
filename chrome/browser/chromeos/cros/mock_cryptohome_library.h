// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Invoke;
using ::testing::WithArgs;
using ::testing::_;

namespace chromeos {

class MockCryptohomeLibrary : public CryptohomeLibrary {
 public:
  MockCryptohomeLibrary();
  virtual ~MockCryptohomeLibrary();

  void SetUp(bool outcome, int code);

  MOCK_METHOD3(AsyncCheckKey, void(const std::string& user_email,
                                   const std::string& passhash,
                                   AsyncMethodCallback callback));
  MOCK_METHOD4(AsyncMigrateKey, void(const std::string& user_email,
                                     const std::string& old_hash,
                                     const std::string& new_hash,
                                     AsyncMethodCallback callback));
  MOCK_METHOD4(AsyncMount, void(const std::string& user_email,
                                const std::string& passhash,
                                const bool create_if_missing,
                                AsyncMethodCallback callback));
  MOCK_METHOD1(AsyncMountGuest, void(AsyncMethodCallback callback));
  MOCK_METHOD2(AsyncRemove, void(const std::string& user_email,
                                 AsyncMethodCallback callback));
  MOCK_METHOD0(IsMounted, bool(void));
  MOCK_METHOD1(HashPassword, std::string(const std::string& password));
  MOCK_METHOD0(GetSystemSalt, std::string(void));

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
  MOCK_METHOD0(InstallAttributesFinalize, bool(void));
  MOCK_METHOD0(InstallAttributesIsReady, bool(void));
  MOCK_METHOD0(InstallAttributesIsInvalid, bool(void));
  MOCK_METHOD0(InstallAttributesIsFirstInstall, bool(void));

  void SetAsyncBehavior(bool outcome, int code) {
    outcome_ = outcome;
    code_ = code;
  }

  void DoCallback(AsyncMethodCallback callback) {
    callback.Run(outcome_, code_);
  }

 private:
  bool outcome_;
  int code_;
  DISALLOW_COPY_AND_ASSIGN(MockCryptohomeLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
