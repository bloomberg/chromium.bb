// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/experiments/api_key_validator.h"

#include "base/time/time.h"
#include "content/common/experiments/api_key.h"

namespace content {

ApiKeyValidator::ApiKeyValidator() {}
ApiKeyValidator::~ApiKeyValidator() {}

bool ApiKeyValidator::validateApiKey(const blink::WebString& apiKey,
                                     const blink::WebString& origin,
                                     const blink::WebString& apiName) {
  bool result = false;
  scoped_ptr<ApiKey> key = ApiKey::Parse(apiKey.utf8());
  if (key) {
    result = (key->IsAppropriate(origin.utf8(), apiName.utf8()) &&
              key->IsValid(base::Time::Now()));
  }
  return result;
}

}  // namespace content
