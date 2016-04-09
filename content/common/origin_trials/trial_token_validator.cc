// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token_validator.h"

#include "base/time/time.h"
#include "content/common/origin_trials/trial_token.h"
#include "content/public/common/content_client.h"

namespace content {

bool TrialTokenValidator::ValidateToken(const std::string& token,
                                        const url::Origin& origin,
                                        base::StringPiece featureName) {
  std::unique_ptr<TrialToken> trial_token = TrialToken::Parse(token);

  // TODO(iclelland): Allow for multiple signing keys, and iterate over all
  // active keys here. https://crbug.com/543220
  ContentClient* content_client = GetContentClient();
  base::StringPiece public_key = content_client->GetOriginTrialPublicKey();

  return !public_key.empty() && trial_token &&
         trial_token->IsAppropriate(origin, featureName) &&
         trial_token->IsValid(base::Time::Now(), public_key);
}

}  // namespace content
