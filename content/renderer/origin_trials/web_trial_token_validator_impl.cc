// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/origin_trials/web_trial_token_validator_impl.h"

#include "base/time/time.h"
#include "third_party/WebKit/common/origin_trials/trial_token.h"
#include "third_party/WebKit/common/origin_trials/trial_token_validator.h"

namespace content {

WebTrialTokenValidatorImpl::WebTrialTokenValidatorImpl(
    std::unique_ptr<blink::TrialTokenValidator> validator)
    : validator_(std::move(validator)) {
  DCHECK(validator_.get()) << "Should not pass null validator.";
}

WebTrialTokenValidatorImpl::~WebTrialTokenValidatorImpl() {}

blink::OriginTrialTokenStatus WebTrialTokenValidatorImpl::ValidateToken(
    const blink::WebString& token,
    const blink::WebSecurityOrigin& origin,
    blink::WebString* feature_name) {
  std::string feature;
  blink::OriginTrialTokenStatus status = validator_->ValidateToken(
      token.Utf8(), origin, &feature, base::Time::Now());
  if (status == blink::OriginTrialTokenStatus::kSuccess)
    *feature_name = blink::WebString::FromUTF8(feature);
  return status;
}

}  // namespace content
