// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_
#pragma once

#include <string>

#include "chromeos/dbus/cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCryptohomeClient : public CryptohomeClient {
 public:
  MockCryptohomeClient();
  virtual ~MockCryptohomeClient();

  MOCK_METHOD1(SetAsyncCallStatusHandler,
               void(const AsyncCallStatusHandler& handler));
  MOCK_METHOD0(ResetAsyncCallStatusHandler, void());
  MOCK_METHOD1(IsMounted, bool(bool* is_mounted));
  MOCK_METHOD1(Unmount, bool(bool* success));
  MOCK_METHOD3(AsyncCheckKey,
               void(const std::string& username,
                    const std::string& key,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(AsyncMigrateKey,
               void(const std::string& username,
                    const std::string& from_key,
                    const std::string& to_key,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD2(AsyncRemove, void(const std::string& username,
                                 const AsyncMethodCallback& callback));
  MOCK_METHOD1(GetSystemSalt, bool(std::vector<uint8>* salt));
  MOCK_METHOD4(AsyncMount, void(const std::string& username,
                                const std::string& key,
                                const bool create_if_missing,
                                const AsyncMethodCallback& callback));
  MOCK_METHOD1(AsyncMountGuest,
               void(const AsyncMethodCallback& callback));
  MOCK_METHOD1(TpmIsReady, bool(bool* ready));
  MOCK_METHOD1(TpmIsEnabled, void(const BoolMethodCallback& callback));
  MOCK_METHOD1(CallTpmIsEnabledAndBlock, bool(bool* enabled));
  MOCK_METHOD1(TpmGetPassword, bool(std::string* password));
  MOCK_METHOD1(TpmIsOwned, bool(bool* owned));
  MOCK_METHOD1(TpmIsBeingOwned, bool(bool* owning));
  MOCK_METHOD0(TpmCanAttemptOwnership, bool());
  MOCK_METHOD0(TpmClearStoredPassword, bool());
  MOCK_METHOD1(Pkcs11IsTpmTokenReady, void(const BoolMethodCallback& callback));
  MOCK_METHOD1(Pkcs11GetTpmTokenInfo,
               void(const Pkcs11GetTpmTokenInfoCallback& callback));
  MOCK_METHOD3(InstallAttributesGet,
               bool(const std::string& name,
                    std::vector<uint8>* value,
                    bool* successful));
  MOCK_METHOD3(InstallAttributesSet,
               bool(const std::string& name,
                    const std::vector<uint8>& value,
                    bool* successful));
  MOCK_METHOD1(InstallAttributesFinalize, bool(bool* successful));
  MOCK_METHOD1(InstallAttributesIsReady, bool(bool* is_ready));
  MOCK_METHOD1(InstallAttributesIsInvalid, bool(bool* is_invalid));
  MOCK_METHOD1(InstallAttributesIsFirstInstall, bool(bool* is_first_install));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_
