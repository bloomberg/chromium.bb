// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/mock_proximity_auth_client.h"

namespace proximity_auth {

MockProximityAuthClient::MockProximityAuthClient() {}

MockProximityAuthClient::~MockProximityAuthClient() {}

scoped_ptr<SecureMessageDelegate>
MockProximityAuthClient::CreateSecureMessageDelegate() {
  return make_scoped_ptr(CreateSecureMessageDelegatePtr());
}

scoped_ptr<CryptAuthClientFactory>
MockProximityAuthClient::CreateCryptAuthClientFactory() {
  return make_scoped_ptr(CreateCryptAuthClientFactoryPtr());
}

}  // proximity_auth
