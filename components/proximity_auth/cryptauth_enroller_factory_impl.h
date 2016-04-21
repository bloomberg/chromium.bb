// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H

#include "base/macros.h"
#include "components/proximity_auth/cryptauth/cryptauth_enroller.h"
#include "components/proximity_auth/proximity_auth_client.h"

namespace proximity_auth {

// Implementation of CryptAuthEnrollerFactory. Note that this class is in the
// proximity_auth/ rather than the cryptauth/ directory because of the
// dependency on ProximityAuthClient.
class CryptAuthEnrollerFactoryImpl : public CryptAuthEnrollerFactory {
 public:
  explicit CryptAuthEnrollerFactoryImpl(
      ProximityAuthClient* proximity_auth_client);
  ~CryptAuthEnrollerFactoryImpl() override;

  // CryptAuthEnrollerFactory:
  std::unique_ptr<CryptAuthEnroller> CreateInstance() override;

 private:
  proximity_auth::ProximityAuthClient* proximity_auth_client_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEnrollerFactoryImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H
