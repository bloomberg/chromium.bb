// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_DOCUMENT_SUBRESOURCE_FILTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "url/gurl.h"

namespace subresource_filter {

class MemoryMappedRuleset;

// Performs filtering of subresource loads in the scope of a given document.
class DocumentSubresourceFilter
    : public blink::WebDocumentSubresourceFilter,
      public base::SupportsWeakPtr<DocumentSubresourceFilter> {
 public:
  // Constructs a new filter that will:
  //  -- Operate at the prescribed |activation_state|, which must be either
  //     ActivationState::DRYRUN or ActivationState::ENABLED. In the former
  //     case filtering will be performed but no loads will be disallowed.
  //  -- Hold a reference to and use |ruleset| for its entire lifetime.
  //  -- Expect |ancestor_document_urls| to be the URLs of documents loaded into
  //     nested frames, starting with the current frame and ending with the main
  //     frame. This provides the context for evaluating domain-specific rules.
  DocumentSubresourceFilter(
      ActivationState activation_state,
      const scoped_refptr<const MemoryMappedRuleset>& ruleset,
      std::vector<GURL> ancestor_document_urls);
  ~DocumentSubresourceFilter() override;

  // blink::WebDocumentSubresourceFilter:
  bool allowLoad(const blink::WebURL& resourceUrl,
                 blink::WebURLRequest::RequestContext) override;

  // Statistics on the number of subresource loads that were evaluated, were
  // matched by filtering rules, and were disallowed, respectively, during the
  // lifetime of |this| filter.
  size_t num_loads_evaluated() const { return num_loads_evaluated_; }
  size_t num_loads_matching_rules() const { return num_loads_matching_rules_; }
  size_t num_loads_disallowed() const { return num_loads_disallowed_; }

 private:
  ActivationState activation_state_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  IndexedRulesetMatcher matcher_;
  std::vector<GURL> ancestor_document_urls_;

  size_t num_loads_evaluated_ = 0;
  size_t num_loads_matching_rules_ = 0;
  size_t num_loads_disallowed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DocumentSubresourceFilter);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_DOCUMENT_SUBRESOURCE_FILTER_H_
