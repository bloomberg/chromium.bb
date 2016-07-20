// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/document_subresource_filter.h"

#include <climits>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"

namespace subresource_filter {

DocumentSubresourceFilter::DocumentSubresourceFilter(
    ActivationState activation_state,
    const scoped_refptr<const MemoryMappedRuleset>& ruleset,
    std::vector<GURL> /* ignored_ancestor_document_urls */)
    : activation_state_(activation_state), ruleset_(ruleset) {
  DCHECK_NE(activation_state_, ActivationState::DISABLED);
  DCHECK(ruleset);
}

DocumentSubresourceFilter::~DocumentSubresourceFilter() = default;

bool DocumentSubresourceFilter::allowLoad(
    const blink::WebURL& resourceUrl,
    blink::WebURLRequest::RequestContext /* ignored */) {
  ++num_loads_evaluated_;
  if (DoesLoadMatchFilteringRules(GURL(resourceUrl))) {
    ++num_loads_matching_rules_;
    if (activation_state_ == ActivationState::ENABLED) {
      ++num_loads_disallowed_;
      return false;
    }
  }
  return true;
}

bool DocumentSubresourceFilter::DoesLoadMatchFilteringRules(
    const GURL& resource_url) {
  static_assert(CHAR_BIT == 8, "Assumed char was 8 bits.");
  base::StringPiece disallowed_suffix = base::StringPiece(
      reinterpret_cast<const char*>(ruleset_->data()), ruleset_->length());
  return !disallowed_suffix.empty() &&
         base::EndsWith(resource_url.path_piece(), disallowed_suffix,
                        base::CompareCase::SENSITIVE);
}

}  // namespace subresource_filter
