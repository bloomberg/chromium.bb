// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ORIGIN_TRIALS_WEB_TRIAL_TOKEN_VALIDATOR_IMPL_H_
#define CONTENT_RENDERER_ORIGIN_TRIALS_WEB_TRIAL_TOKEN_VALIDATOR_IMPL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebTrialTokenValidator.h"

namespace blink {
class TrialTokenValidator;
}

namespace content {

// The TrialTokenValidator is called by the Origin Trials Framework code in
// Blink to validate tokens to enable experimental features.
//
// This class is thread-safe.
// TODO(crbug.com/738505)  Move this to WebKit/core once conversion between
// blink::WebSecurityOrigin and url::Origin is allowed in blink. See
// https://crbug.com/490074
class CONTENT_EXPORT WebTrialTokenValidatorImpl
    : public blink::WebTrialTokenValidator {
 public:
  WebTrialTokenValidatorImpl(
      std::unique_ptr<blink::TrialTokenValidator> validator);
  ~WebTrialTokenValidatorImpl() override;

  // blink::WebTrialTokenValidator implementation
  blink::OriginTrialTokenStatus ValidateToken(
      const blink::WebString& token,
      const blink::WebSecurityOrigin& origin,
      blink::WebString* feature_name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTrialTokenValidatorImpl);

  std::unique_ptr<blink::TrialTokenValidator> validator_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ORIGIN_TRIALS_WEB_TRIAL_TOKEN_VALIDATOR_IMPL_H_
