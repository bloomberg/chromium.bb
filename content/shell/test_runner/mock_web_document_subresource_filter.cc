// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_web_document_subresource_filter.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"

namespace test_runner {

MockWebDocumentSubresourceFilter::MockWebDocumentSubresourceFilter(
    const std::vector<std::string>& disallowed_path_suffixes)
    : disallowed_path_suffixes_(disallowed_path_suffixes) {}

MockWebDocumentSubresourceFilter::~MockWebDocumentSubresourceFilter() {}

blink::WebDocumentSubresourceFilter::LoadPolicy
MockWebDocumentSubresourceFilter::getLoadPolicy(
    const blink::WebURL& resource_url,
    blink::WebURLRequest::RequestContext /* ignored */) {
  const std::string resource_path(GURL(resource_url).path());
  return std::find_if(disallowed_path_suffixes_.begin(),
                      disallowed_path_suffixes_.end(),
                      [&resource_path](const std::string& suffix) {
                        return base::EndsWith(resource_path, suffix,
                                              base::CompareCase::SENSITIVE);
                      }) == disallowed_path_suffixes_.end()
             ? Allow
             : Disallow;
}

void MockWebDocumentSubresourceFilter::reportDisallowedLoad() {}

}  // namespace test_runner
