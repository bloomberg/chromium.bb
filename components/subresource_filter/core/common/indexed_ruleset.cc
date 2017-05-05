// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include "base/logging.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

// RulesetIndexer --------------------------------------------------------------

// static
const int RulesetIndexer::kIndexedFormatVersion = 17;

RulesetIndexer::RulesetIndexer()
    : blacklist_(&builder_), whitelist_(&builder_), deactivation_(&builder_) {}

RulesetIndexer::~RulesetIndexer() = default;

bool RulesetIndexer::AddUrlRule(const proto::UrlRule& rule) {
  const UrlRuleOffset offset = SerializeUrlRule(rule, &builder_);
  // Note: A zero offset.o means a "nullptr" offset. It is returned when the
  // rule has not been serialized.
  if (!offset.o)
    return false;

  if (rule.semantics() == proto::RULE_SEMANTICS_BLACKLIST) {
    blacklist_.IndexUrlRule(offset);
  } else {
    const auto* flat_rule = flatbuffers::GetTemporaryPointer(builder_, offset);
    DCHECK(flat_rule);
    if (flat_rule->element_types())
      whitelist_.IndexUrlRule(offset);
    if (flat_rule->activation_types())
      deactivation_.IndexUrlRule(offset);
  }

  return true;
}

void RulesetIndexer::Finish() {
  auto blacklist_offset = blacklist_.Finish();
  auto whitelist_offset = whitelist_.Finish();
  auto deactivation_offset = deactivation_.Finish();

  auto url_rules_index_offset = flat::CreateIndexedRuleset(
      builder_, blacklist_offset, whitelist_offset, deactivation_offset);
  builder_.Finish(url_rules_index_offset);
}

// IndexedRulesetMatcher -------------------------------------------------------

// static
bool IndexedRulesetMatcher::Verify(const uint8_t* buffer, size_t size) {
  flatbuffers::Verifier verifier(buffer, size);
  return flat::VerifyIndexedRulesetBuffer(verifier);
}

IndexedRulesetMatcher::IndexedRulesetMatcher(const uint8_t* buffer, size_t size)
    : root_(flat::GetIndexedRuleset(buffer)),
      blacklist_(root_->blacklist_index()),
      whitelist_(root_->whitelist_index()),
      deactivation_(root_->deactivation_index()) {}

bool IndexedRulesetMatcher::ShouldDisableFilteringForDocument(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    proto::ActivationType activation_type) const {
  return !!deactivation_.FindMatch(
      document_url, parent_document_origin, proto::ELEMENT_TYPE_UNSPECIFIED,
      activation_type,
      FirstPartyOrigin::IsThirdParty(document_url, parent_document_origin),
      false);
}

bool IndexedRulesetMatcher::ShouldDisallowResourceLoad(
    const GURL& url,
    const FirstPartyOrigin& first_party,
    proto::ElementType element_type,
    bool disable_generic_rules) const {
  const bool is_third_party = first_party.IsThirdParty(url);
  return !!blacklist_.FindMatch(url, first_party.origin(), element_type,
                                proto::ACTIVATION_TYPE_UNSPECIFIED,
                                is_third_party, disable_generic_rules) &&
         !whitelist_.FindMatch(url, first_party.origin(), element_type,
                               proto::ACTIVATION_TYPE_UNSPECIFIED,
                               is_third_party, disable_generic_rules);
}

}  // namespace subresource_filter
