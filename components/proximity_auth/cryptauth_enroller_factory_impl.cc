// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth_enroller_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "components/proximity_auth/cryptauth/cryptauth_client_impl.h"
#include "components/proximity_auth/cryptauth/cryptauth_enroller_impl.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"

namespace proximity_auth {

CryptAuthEnrollerFactoryImpl::CryptAuthEnrollerFactoryImpl(
    ProximityAuthClient* proximity_auth_client)
    : proximity_auth_client_(proximity_auth_client) {}

CryptAuthEnrollerFactoryImpl::~CryptAuthEnrollerFactoryImpl() {}

std::unique_ptr<CryptAuthEnroller>
CryptAuthEnrollerFactoryImpl::CreateInstance() {
  return base::WrapUnique(new CryptAuthEnrollerImpl(
      proximity_auth_client_->CreateCryptAuthClientFactory(),
      proximity_auth_client_->CreateSecureMessageDelegate()));
}

}  // namespace proximity_auth
