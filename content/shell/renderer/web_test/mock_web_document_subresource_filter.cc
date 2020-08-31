// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/mock_web_document_subresource_filter.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"

namespace content {

MockWebDocumentSubresourceFilter::MockWebDocumentSubresourceFilter(
    const std::vector<std::string>& disallowed_path_suffixes,
    bool block_subresources)
    : disallowed_path_suffixes_(disallowed_path_suffixes),
      block_subresources_(block_subresources) {}

MockWebDocumentSubresourceFilter::~MockWebDocumentSubresourceFilter() {}

blink::WebDocumentSubresourceFilter::LoadPolicy
MockWebDocumentSubresourceFilter::GetLoadPolicy(
    const blink::WebURL& resource_url,
    blink::mojom::RequestContextType) {
  return getLoadPolicyImpl(resource_url);
}

blink::WebDocumentSubresourceFilter::LoadPolicy
MockWebDocumentSubresourceFilter::GetLoadPolicyForWebSocketConnect(
    const blink::WebURL& url) {
  return getLoadPolicyImpl(url);
}

blink::WebDocumentSubresourceFilter::LoadPolicy
MockWebDocumentSubresourceFilter::getLoadPolicyImpl(const blink::WebURL& url) {
  const std::string path(GURL(url).path());
  return std::find_if(disallowed_path_suffixes_.begin(),
                      disallowed_path_suffixes_.end(),
                      [&path](const std::string& suffix) {
                        return base::EndsWith(path, suffix,
                                              base::CompareCase::SENSITIVE);
                      }) == disallowed_path_suffixes_.end()
             ? kAllow
             : (block_subresources_ ? kDisallow : kWouldDisallow);
}

void MockWebDocumentSubresourceFilter::ReportDisallowedLoad() {}

bool MockWebDocumentSubresourceFilter::ShouldLogToConsole() {
  return true;
}

}  // namespace content
