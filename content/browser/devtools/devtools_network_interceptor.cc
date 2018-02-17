// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_network_interceptor.h"

namespace content {

InterceptedRequestInfo::InterceptedRequestInfo()
    : response_error_code(net::OK) {}

InterceptedRequestInfo::~InterceptedRequestInfo() = default;

DevToolsNetworkInterceptor::FilterEntry::FilterEntry(
    const base::UnguessableToken& target_id,
    std::vector<Pattern> patterns,
    RequestInterceptedCallback callback)
    : target_id(target_id),
      patterns(std::move(patterns)),
      callback(std::move(callback)) {}

DevToolsNetworkInterceptor::FilterEntry::FilterEntry(FilterEntry&&) {}
DevToolsNetworkInterceptor::FilterEntry::~FilterEntry() {}

}  // namespace content
