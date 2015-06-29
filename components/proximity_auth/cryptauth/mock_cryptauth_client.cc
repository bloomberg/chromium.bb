// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "components/proximity_auth/cryptauth/mock_cryptauth_client.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"

namespace proximity_auth {

MockCryptAuthClient::MockCryptAuthClient() {
}

MockCryptAuthClient::~MockCryptAuthClient() {
}

MockCryptAuthClientFactory::MockCryptAuthClientFactory(bool is_strict)
    : is_strict_(is_strict) {
}

MockCryptAuthClientFactory::~MockCryptAuthClientFactory() {
}

scoped_ptr<CryptAuthClient> MockCryptAuthClientFactory::CreateInstance() {
  scoped_ptr<MockCryptAuthClient> client;
  if (is_strict_)
    client.reset(new testing::StrictMock<MockCryptAuthClient>());
  else
    client.reset(new testing::NiceMock<MockCryptAuthClient>());

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnCryptAuthClientCreated(client.get()));
  return client.Pass();
}

void MockCryptAuthClientFactory::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MockCryptAuthClientFactory::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace proximity_auth
