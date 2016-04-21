// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/mock_proximity_auth_client.h"

#include "base/memory/ptr_util.h"

namespace proximity_auth {

MockProximityAuthClient::MockProximityAuthClient() {}

MockProximityAuthClient::~MockProximityAuthClient() {}

std::unique_ptr<SecureMessageDelegate>
MockProximityAuthClient::CreateSecureMessageDelegate() {
  return base::WrapUnique(CreateSecureMessageDelegatePtr());
}

std::unique_ptr<CryptAuthClientFactory>
MockProximityAuthClient::CreateCryptAuthClientFactory() {
  return base::WrapUnique(CreateCryptAuthClientFactoryPtr());
}

}  // proximity_auth
