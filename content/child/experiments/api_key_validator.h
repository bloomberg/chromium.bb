// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_EXPERIMENTS_API_KEY_VALIDATOR_H_
#define CONTENT_CHILD_EXPERIMENTS_API_KEY_VALIDATOR_H_

#include <string>
#include "third_party/WebKit/public/platform/WebApiKeyValidator.h"

namespace content {

// The ApiKeyValidator is called by the Experimental Framework code in Blink to
// validate tokens to enable experimental APIs.
//
// This class is thread-safe.
class ApiKeyValidator : public blink::WebApiKeyValidator {
 public:
  ApiKeyValidator();
  ~ApiKeyValidator() override;

  // blink::WebApiKeyValidator implementation
  bool validateApiKey(const blink::WebString& apiKey,
                      const blink::WebString& origin,
                      const blink::WebString& apiName) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ApiKeyValidator);
};

}  // namespace content

#endif  // CONTENT_CHILD_EXPERIMENTS_API_KEY_VALIDATOR_H_
