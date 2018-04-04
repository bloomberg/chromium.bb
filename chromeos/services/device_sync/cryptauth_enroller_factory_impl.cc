// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_enroller_factory_impl.h"

#include <memory>

#include "chromeos/services/device_sync/cryptauth_client_factory_impl.h"
#include "components/cryptauth/cryptauth_enroller_impl.h"
#include "components/cryptauth/secure_message_delegate_impl.h"

namespace chromeos {

namespace device_sync {

CryptAuthEnrollerFactoryImpl::CryptAuthEnrollerFactoryImpl(
    identity::IdentityManager* identity_manager,
    scoped_refptr<net::URLRequestContextGetter> url_request_context,
    const cryptauth::DeviceClassifier& device_classifier)
    : identity_manager_(identity_manager),
      url_request_context_(url_request_context),
      device_classifier_(device_classifier) {}

CryptAuthEnrollerFactoryImpl::~CryptAuthEnrollerFactoryImpl() {}

std::unique_ptr<cryptauth::CryptAuthEnroller>
CryptAuthEnrollerFactoryImpl::CreateInstance() {
  return std::make_unique<cryptauth::CryptAuthEnrollerImpl>(
      std::make_unique<CryptAuthClientFactoryImpl>(
          identity_manager_, url_request_context_, device_classifier_),
      cryptauth::SecureMessageDelegateImpl::Factory::NewInstance());
}

}  // namespace device_sync

}  // namespace chromeos
