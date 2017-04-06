// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/fake_cryptauth_service.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/cryptauth/fake_secure_message_delegate.h"
#include "components/cryptauth/mock_cryptauth_client.h"

namespace cryptauth {

FakeCryptAuthService::FakeCryptAuthService() {}

FakeCryptAuthService::~FakeCryptAuthService() {}

CryptAuthDeviceManager* FakeCryptAuthService::GetCryptAuthDeviceManager() {
  return cryptauth_device_manager_;
}

CryptAuthEnrollmentManager*
FakeCryptAuthService::GetCryptAuthEnrollmentManager() {
  return cryptauth_enrollment_manager_;
}

DeviceClassifier FakeCryptAuthService::GetDeviceClassifier() {
  return device_classifier_;
}

std::string FakeCryptAuthService::GetAccountId() {
  return account_id_;
}

std::unique_ptr<SecureMessageDelegate>
FakeCryptAuthService::CreateSecureMessageDelegate() {
  return base::MakeUnique<FakeSecureMessageDelegate>();
}

std::unique_ptr<CryptAuthClientFactory>
FakeCryptAuthService::CreateCryptAuthClientFactory() {
  return base::MakeUnique<MockCryptAuthClientFactory>(
      MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS);
}

}  // namespace cryptauth
