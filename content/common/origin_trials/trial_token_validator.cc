// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token_validator.h"

#include "base/time/time.h"
#include "content/common/origin_trials/trial_token.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/public/platform/WebOriginTrialTokenStatus.h"

namespace content {

blink::WebOriginTrialTokenStatus TrialTokenValidator::ValidateToken(
    const std::string& token,
    const url::Origin& origin,
    base::StringPiece feature_name) {
  // TODO(iclelland): Allow for multiple signing keys, and iterate over all
  // active keys here. https://crbug.com/543220
  ContentClient* content_client = GetContentClient();
  base::StringPiece public_key = content_client->GetOriginTrialPublicKey();
  if (public_key.empty()) {
    return blink::WebOriginTrialTokenStatus::NotSupported;
  }

  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> trial_token =
      TrialToken::From(token, public_key, &status);
  if (status != blink::WebOriginTrialTokenStatus::Success) {
    return status;
  }

  return trial_token->IsValidForFeature(origin, feature_name,
                                        base::Time::Now());
}

}  // namespace content
