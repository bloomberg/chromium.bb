// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "components/subresource_filter/core/common/flat/indexed_ruleset_generated.h"
#include "components/subresource_filter/core/common/url_pattern_index.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

class GURL;

namespace url {
class Origin;
}

namespace subresource_filter {

class FirstPartyOrigin;

namespace proto {
class UrlRule;
}

// The class used to construct flat data structures representing the set of URL
// filtering rules, as well as the index of those. Internally owns a
// FlatBufferBuilder storing the structures.
class RulesetIndexer {
 public:
  // The current binary format version of the indexed ruleset.
  //
  // Increase this value when introducing an incompatible change in
  // IndexedRuleset format, or otherwise willing to nudge clients to rebuild
  // their ruleset (e.g., a change is compatible, but significantly reduces the
  // size of the buffer). Note: The PRESUBMIT.py script tries to keep
  // contributors aware of that.
  static const int kIndexedFormatVersion;

  RulesetIndexer();
  ~RulesetIndexer();

  // Adds |rule| to the ruleset and the index unless the |rule| has unsupported
  // filter options, in which case the data structures remain unmodified.
  // Returns whether the |rule| has been serialized and added to the index.
  bool AddUrlRule(const proto::UrlRule& rule);

  // Finalizes construction of the data structures.
  void Finish();

  // Returns a pointer to the buffer containing the serialized flat data
  // structures. Should only be called after Finish().
  const uint8_t* data() const { return builder_.GetBufferPointer(); }

  // Returns the size of the buffer.
  size_t size() const { return base::strict_cast<size_t>(builder_.GetSize()); }

 private:
  flatbuffers::FlatBufferBuilder builder_;

  UrlPatternIndexBuilder blacklist_;
  UrlPatternIndexBuilder whitelist_;
  UrlPatternIndexBuilder deactivation_;

  DISALLOW_COPY_AND_ASSIGN(RulesetIndexer);
};

// Matches URLs against the FlatBuffer representation of an indexed ruleset.
class IndexedRulesetMatcher {
 public:
  // Returns whether the |buffer| of the given |size| contains a valid
  // flat::IndexedRuleset FlatBuffer.
  static bool Verify(const uint8_t* buffer, size_t size);

  // Creates an instance that matches URLs against the flat::IndexedRuleset
  // provided as the root object of serialized data in the |buffer| of the given
  // |size|.
  IndexedRulesetMatcher(const uint8_t* buffer, size_t size);

  // Returns whether the subset of subresource filtering rules specified by the
  // |activation_type| should be disabled for the |document| loaded from
  // |parent_document_origin|. Always returns false if |activation_type| ==
  // ACTIVATION_TYPE_UNSPECIFIED or the |document_url| is not valid. Unlike
  // page-level activation, such rules can be used to have fine-grained control
  // over the activation of filtering within (sub-)documents.
  bool ShouldDisableFilteringForDocument(
      const GURL& document_url,
      const url::Origin& parent_document_origin,
      proto::ActivationType activation_type) const;

  // Returns whether the network request to |url| of |element_type| initiated by
  // |document_origin| is not allowed to proceed. Always returns false if the
  // |url| is not valid or |element_type| == ELEMENT_TYPE_UNSPECIFIED.
  bool ShouldDisallowResourceLoad(const GURL& url,
                                  const FirstPartyOrigin& first_party,
                                  proto::ElementType element_type,
                                  bool disable_generic_rules) const;

 private:
  const flat::IndexedRuleset* root_;

  UrlPatternIndexMatcher blacklist_;
  UrlPatternIndexMatcher whitelist_;
  UrlPatternIndexMatcher deactivation_;

  DISALLOW_COPY_AND_ASSIGN(IndexedRulesetMatcher);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_
