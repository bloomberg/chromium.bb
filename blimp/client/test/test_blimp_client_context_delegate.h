// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_TEST_TEST_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
#define BLIMP_CLIENT_TEST_TEST_BLIMP_CLIENT_CONTEXT_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/identity_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace client {
class BlimpContents;

// Helper class to use in tests when a BlimpClientContextDelegate is needed.
class TestBlimpClientContextDelegate : public BlimpClientContextDelegate {
 public:
  TestBlimpClientContextDelegate();
  ~TestBlimpClientContextDelegate() override;

  // BlimpClientContextDelegate implementation.
  MOCK_METHOD1(AttachBlimpContentsHelpers, void(BlimpContents*));
  MOCK_METHOD2(OnAssignmentConnectionAttempted,
               void(AssignmentRequestResult, const Assignment&));
  std::unique_ptr<IdentityProvider> CreateIdentityProvider() override;
  MOCK_METHOD1(OnAuthenticationError, void(const GoogleServiceAuthError&));
  MOCK_METHOD0(OnConnected, void());
  MOCK_METHOD1(OnEngineDisconnected, void(int));
  MOCK_METHOD1(OnNetworkDisconnected, void(int));

  FakeOAuth2TokenService* GetTokenService();

 private:
  std::unique_ptr<FakeOAuth2TokenService> token_service_;
  DISALLOW_COPY_AND_ASSIGN(TestBlimpClientContextDelegate);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_TEST_TEST_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
