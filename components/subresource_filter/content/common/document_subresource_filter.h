// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/common/document_load_statistics.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

class FirstPartyOrigin;
class MemoryMappedRuleset;

// Computes whether/how subresource filtering should be activated while loading
// |document_url| in a frame, based on the parent document's |activation_state|,
// the |parent_document_origin|, as well as any applicable deactivation rules in
// non-null |ruleset|.
ActivationState ComputeActivationState(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    const ActivationState& parent_activation_state,
    const MemoryMappedRuleset* ruleset);

// Same as above, but instead of relying on the pre-computed activation state of
// the parent, computes the activation state for a frame from scratch, based on
// the page-level activation options |activation_level| and
// |measure_performance|; as well as any deactivation rules in |ruleset| that
// apply to |ancestor_document_urls|.
//
// TODO(pkalinnikov): Remove this when browser-side navigation is supported.
ActivationState ComputeActivationState(
    ActivationLevel activation_level,
    bool measure_performance,
    const std::vector<GURL>& ancestor_document_urls,
    const MemoryMappedRuleset* ruleset);

// Performs filtering of subresource loads in the scope of a given document.
class DocumentSubresourceFilter
    : public blink::WebDocumentSubresourceFilter,
      public base::SupportsWeakPtr<DocumentSubresourceFilter> {
 public:
  // Constructs a new filter that will:
  //  -- Operate in a manner prescribed in |activation_state|.
  //  -- Filter subresource loads in the scope of a document loaded from
  //     |document_origin|.
  //  -- Hold a reference to and use |ruleset| for its entire lifetime.
  //  -- Invoke |first_disallowed_load_callback|, if it is non-null, on the
  //     first disallowed subresource load.
  DocumentSubresourceFilter(url::Origin document_origin,
                            ActivationState activation_state,
                            scoped_refptr<const MemoryMappedRuleset> ruleset,
                            base::OnceClosure first_disallowed_load_callback);

  ~DocumentSubresourceFilter() override;

  const DocumentLoadStatistics& statistics() const { return statistics_; }
  bool is_performance_measuring_enabled() const {
    return activation_state_.measure_performance;
  }

  // blink::WebDocumentSubresourceFilter:
  LoadPolicy getLoadPolicy(const blink::WebURL& resourceUrl,
                           blink::WebURLRequest::RequestContext) override;
  void reportDisallowedLoad() override;

  LoadPolicy GetLoadPolicyForSubdocument(const GURL& subdocument_url);

 private:
  LoadPolicy EvaluateLoadPolicy(const GURL& resource_url,
                                proto::ElementType element_type);

  const ActivationState activation_state_;
  const scoped_refptr<const MemoryMappedRuleset> ruleset_;
  const IndexedRulesetMatcher ruleset_matcher_;

  // Equals nullptr iff |activation_state_.filtering_disabled_for_document|.
  std::unique_ptr<FirstPartyOrigin> document_origin_;

  base::OnceClosure first_disallowed_load_callback_;
  DocumentLoadStatistics statistics_;

  DISALLOW_COPY_AND_ASSIGN(DocumentSubresourceFilter);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_DOCUMENT_SUBRESOURCE_FILTER_H_
