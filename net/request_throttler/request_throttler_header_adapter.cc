// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/request_throttler/request_throttler_header_adapter.h"

RequestThrottlerHeaderAdapter::RequestThrottlerHeaderAdapter(
    const scoped_refptr<net::HttpResponseHeaders>& headers)
    : response_header_(headers) {
}

std::string RequestThrottlerHeaderAdapter::GetNormalizedValue(
    const std::string& key) const {
  std::string return_value;
  response_header_->GetNormalizedHeader(key, &return_value);
  return return_value;
}

int RequestThrottlerHeaderAdapter::GetResponseCode() const {
  return response_header_->response_code();
}
