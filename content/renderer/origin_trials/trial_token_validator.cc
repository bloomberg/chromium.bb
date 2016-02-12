// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/origin_trials/trial_token_validator.h"

#include "base/time/time.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/origin_trials/trial_token.h"

namespace content {

TrialTokenValidator::TrialTokenValidator() {}
TrialTokenValidator::~TrialTokenValidator() {}

bool TrialTokenValidator::validateToken(const blink::WebString& token,
                                        const blink::WebString& origin,
                                        const blink::WebString& featureName) {
  scoped_ptr<TrialToken> trial_token = TrialToken::Parse(token.utf8());

  ContentClient* content_client = GetContentClient();
  CHECK(content_client);

  base::StringPiece public_key =
      content_client->renderer()->GetOriginTrialPublicKey();

  return !public_key.empty() && trial_token &&
         trial_token->IsAppropriate(origin.utf8(), featureName.utf8()) &&
         trial_token->IsValid(base::Time::Now(), public_key);
}

}  // namespace content
