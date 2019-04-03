// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"

#include <string>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;
namespace flat_rule = url_pattern_index::flat;

template <typename T>
using FlatOffset = flatbuffers::Offset<T>;

template <typename T>
using FlatVectorOffset = FlatOffset<flatbuffers::Vector<FlatOffset<T>>>;

using FlatStringOffset = FlatOffset<flatbuffers::String>;
using FlatStringListOffset = FlatVectorOffset<flatbuffers::String>;

// Writes to |builder| a flatbuffer vector of shared strings corresponding to
// |vec| and returns the offset to it. If |vec| is empty, returns an empty
// offset.
FlatStringListOffset BuildVectorOfSharedStrings(
    flatbuffers::FlatBufferBuilder* builder,
    const std::vector<std::string>& vec) {
  if (vec.empty())
    return FlatStringListOffset();

  std::vector<FlatStringOffset> offsets;
  offsets.reserve(vec.size());
  for (const auto& str : vec)
    offsets.push_back(builder->CreateSharedString(str));
  return builder->CreateVector(offsets);
}

// Returns the RuleActionType corresponding to |indexed_rule|.
dnr_api::RuleActionType GetRuleActionType(const IndexedRule& indexed_rule) {
  if (indexed_rule.options & flat_rule::OptionFlag_IS_WHITELIST) {
    DCHECK(indexed_rule.redirect_url.empty());
    return dnr_api::RULE_ACTION_TYPE_ALLOW;
  }
  if (!indexed_rule.redirect_url.empty())
    return dnr_api::RULE_ACTION_TYPE_REDIRECT;
  return dnr_api::RULE_ACTION_TYPE_BLOCK;
}

}  // namespace

FlatRulesetIndexer::FlatRulesetIndexer() {
  index_builders_.reserve(flat::ActionIndex_count);
  for (size_t i = 0; i < flat::ActionIndex_count; ++i) {
    index_builders_.push_back(
        std::make_unique<UrlPatternIndexBuilder>(&builder_));
  }
}

FlatRulesetIndexer::~FlatRulesetIndexer() = default;

void FlatRulesetIndexer::AddUrlRule(const IndexedRule& indexed_rule) {
  DCHECK(!finished_);

  ++indexed_rules_count_;

  FlatStringListOffset domains_included_offset =
      BuildVectorOfSharedStrings(&builder_, indexed_rule.domains);
  FlatStringListOffset domains_excluded_offset =
      BuildVectorOfSharedStrings(&builder_, indexed_rule.excluded_domains);
  FlatStringOffset url_pattern_offset =
      builder_.CreateSharedString(indexed_rule.url_pattern);

  FlatOffset<flat_rule::UrlRule> offset = flat_rule::CreateUrlRule(
      builder_, indexed_rule.options, indexed_rule.element_types,
      indexed_rule.activation_types, indexed_rule.url_pattern_type,
      indexed_rule.anchor_left, indexed_rule.anchor_right,
      domains_included_offset, domains_excluded_offset, url_pattern_offset,
      indexed_rule.id, indexed_rule.priority);
  const dnr_api::RuleActionType type = GetRuleActionType(indexed_rule);
  GetBuilder(type)->IndexUrlRule(offset);

  // Store additional metadata required for a redirect rule.
  if (type == dnr_api::RULE_ACTION_TYPE_REDIRECT) {
    DCHECK(!indexed_rule.redirect_url.empty());
    FlatStringOffset redirect_url_offset =
        builder_.CreateSharedString(indexed_rule.redirect_url);
    metadata_.push_back(flat::CreateUrlRuleMetadata(builder_, indexed_rule.id,
                                                    redirect_url_offset));
  }
}

void FlatRulesetIndexer::Finish() {
  DCHECK(!finished_);
  finished_ = true;

  std::vector<url_pattern_index::UrlPatternIndexOffset> index_offsets;
  index_offsets.reserve(index_builders_.size());
  for (const auto& builder : index_builders_)
    index_offsets.push_back(builder->Finish());

  FlatVectorOffset<url_pattern_index::flat::UrlPatternIndex>
      index_vector_offset = builder_.CreateVector(index_offsets);

  // Store the extension metadata sorted by ID to support fast lookup through
  // binary search.
  FlatVectorOffset<flat::UrlRuleMetadata> extension_metadata_offset =
      builder_.CreateVectorOfSortedTables(&metadata_);

  FlatOffset<flat::ExtensionIndexedRuleset> root_offset =
      flat::CreateExtensionIndexedRuleset(builder_, index_vector_offset,
                                          extension_metadata_offset);
  flat::FinishExtensionIndexedRulesetBuffer(builder_, root_offset);
}

base::span<const uint8_t> FlatRulesetIndexer::GetData() {
  DCHECK(finished_);
  return base::make_span(builder_.GetBufferPointer(), builder_.GetSize());
}

FlatRulesetIndexer::UrlPatternIndexBuilder* FlatRulesetIndexer::GetBuilder(
    dnr_api::RuleActionType type) {
  switch (type) {
    case dnr_api::RULE_ACTION_TYPE_BLOCK:
      return index_builders_[flat::ActionIndex_block].get();
    case dnr_api::RULE_ACTION_TYPE_ALLOW:
      return index_builders_[flat::ActionIndex_allow].get();
    case dnr_api::RULE_ACTION_TYPE_REDIRECT:
      return index_builders_[flat::ActionIndex_redirect].get();
    case dnr_api::RULE_ACTION_TYPE_NONE:
      NOTREACHED();
  }
  return nullptr;
}

}  // namespace declarative_net_request
}  // namespace extensions
