// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/origin_trials/trial_token_validator.h"

#include "base/time/time.h"
#include "content/renderer/origin_trials/trial_token.h"

namespace content {

TrialTokenValidator::TrialTokenValidator() {}
TrialTokenValidator::~TrialTokenValidator() {}

bool TrialTokenValidator::validateToken(const blink::WebString& token,
                                        const blink::WebString& origin,
                                        const blink::WebString& featureName) {
  scoped_ptr<TrialToken> trialToken = TrialToken::Parse(token.utf8());
  return trialToken &&
         trialToken->IsAppropriate(origin.utf8(), featureName.utf8()) &&
         trialToken->IsValid(base::Time::Now());
}

}  // namespace content
