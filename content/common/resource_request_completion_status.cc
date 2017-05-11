// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resource_request_completion_status.h"

#include "net/base/net_errors.h"

namespace content {

ResourceRequestCompletionStatus::ResourceRequestCompletionStatus() {}
ResourceRequestCompletionStatus::ResourceRequestCompletionStatus(
    const ResourceRequestCompletionStatus& status) = default;

ResourceRequestCompletionStatus::ResourceRequestCompletionStatus(int64_t length)
    : error_code(net::OK),
      completion_time(base::TimeTicks::Now()),
      encoded_data_length(length),
      encoded_body_length(length) {}

ResourceRequestCompletionStatus::~ResourceRequestCompletionStatus() {}

}  // namespace content
