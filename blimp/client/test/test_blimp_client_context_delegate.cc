// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/test/test_blimp_client_context_delegate.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/public/blimp_client_context_delegate.h"

namespace blimp {
namespace client {

TestBlimpClientContextDelegate::TestBlimpClientContextDelegate() {
  token_service_ =
      base::WrapUnique<FakeOAuth2TokenService>(new FakeOAuth2TokenService());
}

TestBlimpClientContextDelegate::~TestBlimpClientContextDelegate() = default;

std::unique_ptr<IdentityProvider>
TestBlimpClientContextDelegate::CreateIdentityProvider() {
  return base::WrapUnique<IdentityProvider>(
      new FakeIdentityProvider(token_service_.get()));
}

FakeOAuth2TokenService* TestBlimpClientContextDelegate::GetTokenService() {
  return token_service_.get();
}

}  // namespace client
}  // namespace blimp
