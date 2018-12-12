// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_

#include "chromeos/services/device_sync/cryptauth_enroller.h"

namespace chromeos {

namespace device_sync {

class CryptAuthClientFactory;

// CryptAuthEnrollerFactory implementation which utilizes IdentityManager.
class CryptAuthEnrollerFactoryImpl : public CryptAuthEnrollerFactory {
 public:
  CryptAuthEnrollerFactoryImpl(
      CryptAuthClientFactory* cryptauth_client_factory);
  ~CryptAuthEnrollerFactoryImpl() override;

  // CryptAuthEnrollerFactory:
  std::unique_ptr<CryptAuthEnroller> CreateInstance() override;

 private:
  CryptAuthClientFactory* cryptauth_client_factory_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_
