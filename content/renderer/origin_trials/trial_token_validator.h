// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_
#define CONTENT_RENDERER_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_

#include <string>
#include "third_party/WebKit/public/platform/WebTrialTokenValidator.h"

namespace content {

// The TrialTokenValidator is called by the Experimental Framework code in Blink
// to validate tokens to enable experimental features.
//
// This class is thread-safe.
class TrialTokenValidator : public blink::WebTrialTokenValidator {
 public:
  TrialTokenValidator();
  ~TrialTokenValidator() override;

  // blink::WebTrialTokenValidator implementation
  bool validateToken(const blink::WebString& token,
                     const blink::WebString& origin,
                     const blink::WebString& featureName) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrialTokenValidator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_
