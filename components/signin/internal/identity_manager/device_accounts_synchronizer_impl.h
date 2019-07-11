// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_

#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"

class OAuth2TokenServiceDelegate;

namespace identity {

// Concrete implementation of DeviceAccountsSynchronizer interface.
class DeviceAccountsSynchronizerImpl : public DeviceAccountsSynchronizer {
 public:
  explicit DeviceAccountsSynchronizerImpl(
      OAuth2TokenServiceDelegate* token_service_delegate);
  ~DeviceAccountsSynchronizerImpl() override;

  // DeviceAccountsSynchronizer implementation.
  void ReloadAllAccountsFromSystem() override;
  void ReloadAccountFromSystem(const CoreAccountId& account_id) override;

 private:
  OAuth2TokenServiceDelegate* token_service_delegate_ = nullptr;
};

}  // namespace identity

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_IMPL_H_
