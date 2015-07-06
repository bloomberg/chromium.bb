// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/webui/cryptauth_enroller_factory_impl.h"

#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/cryptauth/cryptauth_enroller_impl.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"
#include "components/proximity_auth/webui/proximity_auth_ui_delegate.h"

namespace proximity_auth {

CryptAuthEnrollerFactoryImpl::CryptAuthEnrollerFactoryImpl(
    ProximityAuthUIDelegate* delegate)
    : delegate_(delegate) {
}

CryptAuthEnrollerFactoryImpl::~CryptAuthEnrollerFactoryImpl() {
}

scoped_ptr<CryptAuthEnroller> CryptAuthEnrollerFactoryImpl::CreateInstance() {
  return make_scoped_ptr(
      new CryptAuthEnrollerImpl(delegate_->CreateCryptAuthClientFactory(),
                                delegate_->CreateSecureMessageDelegate()));
}

}  // namespace proximity_auth
