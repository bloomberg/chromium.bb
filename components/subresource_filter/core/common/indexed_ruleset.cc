// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/ngram_extractor.h"
#include "components/subresource_filter/core/common/url_pattern.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace subresource_filter {

namespace {

using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;
using FlatDomains = flatbuffers::Vector<FlatStringOffset>;
using FlatDomainsOffset = flatbuffers::Offset<FlatDomains>;

base::StringPiece ToStringPiece(const flatbuffers::String* string) {
  DCHECK(string);
  return base::StringPiece(string->c_str(), string->size());
}

// Performs three-way comparison between two domains. In the total order defined
// by this predicate, the lengths of domains will be monotonically decreasing.
int CompareDomains(base::StringPiece lhs_domain, base::StringPiece rhs_domain) {
  if (lhs_domain.size() != rhs_domain.size())
    return lhs_domain.size() > rhs_domain.size() ? -1 : 1;
  return lhs_domain.compare(rhs_domain);
}

bool HasNoUpperAscii(base::StringPiece string) {
  return std::none_of(string.begin(), string.end(),
                      [](char c) { return base::IsAsciiUpper(c); });
}

// Checks whether a URL |rule| can be converted to its FlatBuffers equivalent,
// and performs the actual conversion.
class UrlRuleFlatBufferConverter {
 public:
  // Creates the converter, and initializes |is_convertible| bit. If
  // |is_convertible| == true, then all the fields, needed for serializing the
  // |rule| to FlatBuffer, are initialized (|options|, |anchor_right|, etc.).
  UrlRuleFlatBufferConverter(const proto::UrlRule& rule) : rule_(rule) {
    is_convertible_ = InitializeOptions() && InitializeElementTypes() &&
                      InitializeActivationTypes() && InitializeUrlPattern() &&
                      IsMeaningful();
  }

  // Returns whether the |rule| can be converted to its FlatBuffers equivalent.
  // The conversion is not possible if the rule has attributes not supported by
  // this client version.
  bool is_convertible() const { return is_convertible_; }

  bool has_element_types() const { return !!element_types_; }
  bool has_activation_types() const { return !!activation_types_; }

  // Writes the URL |rule| to the FlatBuffer using the |builder|, and returns
  // the offset to the serialized rule.
  flatbuffers::Offset<flat::UrlRule> SerializeConvertedRule(
      flatbuffers::FlatBufferBuilder* builder) const {
    DCHECK(is_convertible());

    FlatDomainsOffset domains_included_offset;
    FlatDomainsOffset domains_excluded_offset;
    if (rule_.domains_size()) {
      // TODO(pkalinnikov): Consider sharing the vectors between rules.
      std::vector<FlatStringOffset> domains_included;
      std::vector<FlatStringOffset> domains_excluded;
      // Reserve only for |domains_included| because it is expected to be the
      // one used more frequently.
      domains_included.reserve(rule_.domains_size());

      for (const auto& domain_list_item : rule_.domains()) {
        // Note: The |domain| can have non-ASCII UTF-8 characters, but
        // ToLowerASCII leaves these intact.
        // TODO(pkalinnikov): Convert non-ASCII characters to lower case too.
        // TODO(pkalinnikov): Possibly convert Punycode to IDN here or directly
        // assume this is done in the proto::UrlRule.
        const std::string& domain = domain_list_item.domain();
        auto offset = builder->CreateSharedString(
            HasNoUpperAscii(domain) ? domain : base::ToLowerASCII(domain));

        if (domain_list_item.exclude())
          domains_excluded.push_back(offset);
        else
          domains_included.push_back(offset);
      }

      // The comparator ensuring the domains order necessary for fast matching.
      auto precedes = [&builder](FlatStringOffset lhs, FlatStringOffset rhs) {
        return CompareDomains(ToStringPiece(flatbuffers::GetTemporaryPointer(
                                  *builder, lhs)),
                              ToStringPiece(flatbuffers::GetTemporaryPointer(
                                  *builder, rhs))) < 0;
      };

      // The domains are stored in sorted order to support fast matching.
      if (!domains_included.empty()) {
        // TODO(pkalinnikov): Don't sort if it is already sorted offline.
        std::sort(domains_included.begin(), domains_included.end(), precedes);
        domains_included_offset = builder->CreateVector(domains_included);
      }
      if (!domains_excluded.empty()) {
        std::sort(domains_excluded.begin(), domains_excluded.end(), precedes);
        domains_excluded_offset = builder->CreateVector(domains_excluded);
      }
    }

    auto url_pattern_offset = builder->CreateString(rule_.url_pattern());

    return flat::CreateUrlRule(
        *builder, options_, element_types_, activation_types_,
        url_pattern_type_, anchor_left_, anchor_right_, domains_included_offset,
        domains_excluded_offset, url_pattern_offset);
  }

 private:
  static bool ConvertAnchorType(proto::AnchorType anchor_type,
                                flat::AnchorType* result) {
    switch (anchor_type) {
      case proto::ANCHOR_TYPE_NONE:
        *result = flat::AnchorType_NONE;
        break;
      case proto::ANCHOR_TYPE_BOUNDARY:
        *result = flat::AnchorType_BOUNDARY;
        break;
      case proto::ANCHOR_TYPE_SUBDOMAIN:
        *result = flat::AnchorType_SUBDOMAIN;
        break;
      default:
        return false;  // Unsupported anchor type.
    }
    return true;
  }

  bool InitializeOptions() {
    if (rule_.semantics() == proto::RULE_SEMANTICS_WHITELIST) {
      options_ |= flat::OptionFlag_IS_WHITELIST;
    } else if (rule_.semantics() != proto::RULE_SEMANTICS_BLACKLIST) {
      return false;  // Unsupported semantics.
    }

    switch (rule_.source_type()) {
      case proto::SOURCE_TYPE_ANY:
        options_ |= flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
      // Note: fall through here intentionally.
      case proto::SOURCE_TYPE_FIRST_PARTY:
        options_ |= flat::OptionFlag_APPLIES_TO_FIRST_PARTY;
        break;
      case proto::SOURCE_TYPE_THIRD_PARTY:
        options_ |= flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
        break;

      default:
        return false;  // Unsupported source type.
    }

    if (rule_.match_case())
      options_ |= flat::OptionFlag_IS_MATCH_CASE;

    return true;
  }

  bool InitializeElementTypes() {
    static_assert(
        proto::ELEMENT_TYPE_ALL <= std::numeric_limits<uint16_t>::max(),
        "Element types can not be stored in uint16_t.");
    element_types_ = static_cast<uint16_t>(rule_.element_types());

    // Note: Normally we can not distinguish between the main plugin resource
    // and any other loads it makes. We treat them both as OBJECT requests.
    if (element_types_ & proto::ELEMENT_TYPE_OBJECT_SUBREQUEST)
      element_types_ |= proto::ELEMENT_TYPE_OBJECT;

    // Ignore unknown element types.
    element_types_ &= proto::ELEMENT_TYPE_ALL;
    // Filtering popups is not supported.
    element_types_ &= ~proto::ELEMENT_TYPE_POPUP;

    return true;
  }

  bool InitializeActivationTypes() {
    static_assert(
        proto::ACTIVATION_TYPE_ALL <= std::numeric_limits<uint8_t>::max(),
        "Activation types can not be stored in uint8_t.");
    activation_types_ = static_cast<uint8_t>(rule_.activation_types());

    // Ignore unknown activation types.
    activation_types_ &= proto::ACTIVATION_TYPE_ALL;
    // No need in CSS activation, because the CSS rules are not supported.
    activation_types_ &=
        ~(proto::ACTIVATION_TYPE_ELEMHIDE | proto::ACTIVATION_TYPE_GENERICHIDE);

    return true;
  }

  bool InitializeUrlPattern() {
    switch (rule_.url_pattern_type()) {
      case proto::URL_PATTERN_TYPE_SUBSTRING:
        url_pattern_type_ = flat::UrlPatternType_SUBSTRING;
        break;
      case proto::URL_PATTERN_TYPE_WILDCARDED:
        url_pattern_type_ = flat::UrlPatternType_WILDCARDED;
        break;

      // TODO(pkalinnikov): Implement REGEXP rules matching.
      case proto::URL_PATTERN_TYPE_REGEXP:
      default:
        return false;  // Unsupported URL pattern type.
    }

    if (!ConvertAnchorType(rule_.anchor_left(), &anchor_left_) ||
        !ConvertAnchorType(rule_.anchor_right(), &anchor_right_)) {
      return false;
    }
    if (anchor_right_ == flat::AnchorType_SUBDOMAIN)
      return false;  // Unsupported right anchor.

    return true;
  }

  // Returns whether the rule is not a no-op after all the modifications above.
  bool IsMeaningful() const { return element_types_ || activation_types_; }

  const proto::UrlRule& rule_;

  uint8_t options_ = 0;
  uint16_t element_types_ = 0;
  uint8_t activation_types_ = 0;
  flat::UrlPatternType url_pattern_type_ = flat::UrlPatternType_WILDCARDED;
  flat::AnchorType anchor_left_ = flat::AnchorType_NONE;
  flat::AnchorType anchor_right_ = flat::AnchorType_NONE;

  bool is_convertible_ = true;
};

}  // namespace

// RulesetIndexer --------------------------------------------------------------

// static
const int RulesetIndexer::kIndexedFormatVersion = 17;

RulesetIndexer::MutableUrlPatternIndex::MutableUrlPatternIndex() = default;
RulesetIndexer::MutableUrlPatternIndex::~MutableUrlPatternIndex() = default;

RulesetIndexer::RulesetIndexer() = default;
RulesetIndexer::~RulesetIndexer() = default;

bool RulesetIndexer::AddUrlRule(const proto::UrlRule& rule) {
  UrlRuleFlatBufferConverter converter(rule);
  if (!converter.is_convertible())
    return false;
  DCHECK_NE(rule.url_pattern_type(), proto::URL_PATTERN_TYPE_REGEXP);
  auto rule_offset = converter.SerializeConvertedRule(&builder_);

  auto add_rule_to_index = [&rule, rule_offset](MutableUrlPatternIndex* index) {
    NGram ngram =
        GetMostDistinctiveNGram(index->ngram_index, rule.url_pattern());
    if (ngram) {
      index->ngram_index[ngram].push_back(rule_offset);
    } else {
      // TODO(pkalinnikov): Index fallback rules as well.
      index->fallback_rules.push_back(rule_offset);
    }
  };

  if (rule.semantics() == proto::RULE_SEMANTICS_BLACKLIST) {
    add_rule_to_index(&blacklist_);
  } else {
    if (converter.has_element_types())
      add_rule_to_index(&whitelist_);
    if (converter.has_activation_types())
      add_rule_to_index(&activation_);
  }

  return true;
}

void RulesetIndexer::Finish() {
  auto blacklist_offset = SerializeUrlPatternIndex(blacklist_);
  auto whitelist_offset = SerializeUrlPatternIndex(whitelist_);
  auto activation_offset = SerializeUrlPatternIndex(activation_);

  auto url_rules_index_offset = flat::CreateIndexedRuleset(
      builder_, blacklist_offset, whitelist_offset, activation_offset);
  builder_.Finish(url_rules_index_offset);
}

// static
NGram RulesetIndexer::GetMostDistinctiveNGram(
    const MutableNGramIndex& ngram_index,
    base::StringPiece pattern) {
  size_t min_list_size = std::numeric_limits<size_t>::max();
  NGram best_ngram = 0;

  auto ngrams = CreateNGramExtractor<kNGramSize, NGram>(
      pattern, [](char c) { return c == '*' || c == '^'; });

  for (uint64_t ngram : ngrams) {
    const MutableUrlRuleList* rules = ngram_index.Get(ngram);
    const size_t list_size = rules ? rules->size() : 0;
    if (list_size < min_list_size) {
      // TODO(pkalinnikov): Pick random of the same-sized lists.
      min_list_size = list_size;
      best_ngram = ngram;
      if (list_size == 0)
        break;
    }
  }

  return best_ngram;
}

flatbuffers::Offset<flat::UrlPatternIndex>
RulesetIndexer::SerializeUrlPatternIndex(const MutableUrlPatternIndex& index) {
  const MutableNGramIndex& ngram_index = index.ngram_index;

  std::vector<flatbuffers::Offset<flat::NGramToRules>> flat_hash_table(
      ngram_index.table_size());

  flatbuffers::Offset<flat::NGramToRules> empty_slot_offset =
      flat::CreateNGramToRules(builder_);
  for (size_t i = 0, size = ngram_index.table_size(); i != size; ++i) {
    const uint32_t entry_index = ngram_index.hash_table()[i];
    if (entry_index >= ngram_index.size()) {
      flat_hash_table[i] = empty_slot_offset;
      continue;
    }
    const MutableNGramIndex::EntryType& entry =
        ngram_index.entries()[entry_index];
    auto rules_offset = builder_.CreateVector(entry.second);
    flat_hash_table[i] =
        flat::CreateNGramToRules(builder_, entry.first, rules_offset);
  }
  auto ngram_index_offset = builder_.CreateVector(flat_hash_table);

  auto fallback_rules_offset = builder_.CreateVector(index.fallback_rules);

  return flat::CreateUrlPatternIndex(builder_, kNGramSize, ngram_index_offset,
                                     empty_slot_offset, fallback_rules_offset);
}

// IndexedRulesetMatcher -------------------------------------------------------

namespace {

using FlatUrlRuleList = flatbuffers::Vector<flatbuffers::Offset<flat::UrlRule>>;
using FlatNGramIndex =
    flatbuffers::Vector<flatbuffers::Offset<flat::NGramToRules>>;

// Returns the size of the longest (sub-)domain of |origin| matching one of the
// |domains| in the list.
//
// The |domains| should be sorted in descending order of their length, and
// ascending alphabetical order within the groups of same-length domains.
size_t GetLongestMatchingSubdomain(const url::Origin& origin,
                                   const FlatDomains& domains) {
  // If the |domains| list is short, then the simple strategy is usually faster.
  if (domains.size() <= 5) {
    for (auto* domain : domains) {
      const base::StringPiece domain_piece = ToStringPiece(domain);
      if (origin.DomainIs(domain_piece))
        return domain_piece.size();
    }
    return 0;
  }
  // Otherwise look for each subdomain of the |origin| using binary search.

  DCHECK(!origin.unique());
  base::StringPiece canonicalized_host(origin.host());
  if (canonicalized_host.empty())
    return 0;

  // If the host name ends with a dot, then ignore it.
  if (canonicalized_host.back() == '.')
    canonicalized_host.remove_suffix(1);

  // The |left| bound of the search is shared between iterations, because
  // subdomains are considered in decreasing order of their lengths, therefore
  // each consecutive lower_bound will be at least as far as the previous.
  flatbuffers::uoffset_t left = 0;
  for (size_t position = 0;; ++position) {
    const base::StringPiece subdomain = canonicalized_host.substr(position);

    flatbuffers::uoffset_t right = domains.size();
    while (left + 1 < right) {
      auto middle = left + (right - left) / 2;
      DCHECK_LT(middle, domains.size());
      if (CompareDomains(ToStringPiece(domains[middle]), subdomain) <= 0)
        left = middle;
      else
        right = middle;
    }

    DCHECK_LT(left, domains.size());
    if (ToStringPiece(domains[left]) == subdomain)
      return subdomain.size();

    position = canonicalized_host.find('.', position);
    if (position == base::StringPiece::npos)
      break;
  }

  return 0;
}

// Returns whether the |origin| matches the domain list of the |rule|. A match
// means that the longest domain in |domains| that |origin| is a sub-domain of
// is not an exception OR all the |domains| are exceptions and neither matches
// the |origin|. Thus, domain filters with more domain components trump filters
// with fewer domain components, i.e. the more specific a filter is, the higher
// the priority.
//
// A rule whose domain list is empty or contains only negative domains is still
// considered a "generic" rule. Therefore, if |disable_generic_rules| is set,
// this function will always return false for such rules.
bool DoesOriginMatchDomainList(const url::Origin& origin,
                               const flat::UrlRule& rule,
                               bool disable_generic_rules) {
  const bool is_generic = !rule.domains_included();
  DCHECK(is_generic || rule.domains_included()->size());
  if (disable_generic_rules && is_generic)
    return false;

  // Unique |origin| matches lists of exception domains only.
  if (origin.unique())
    return is_generic;

  size_t longest_matching_included_domain_length = 1;
  if (!is_generic) {
    longest_matching_included_domain_length =
        GetLongestMatchingSubdomain(origin, *rule.domains_included());
  }
  if (longest_matching_included_domain_length && rule.domains_excluded()) {
    return GetLongestMatchingSubdomain(origin, *rule.domains_excluded()) <
           longest_matching_included_domain_length;
  }
  return !!longest_matching_included_domain_length;
}

// Returns whether the request matches flags of the specified URL |rule|. Takes
// into account:
//  - |element_type| of the requested resource, if not *_UNSPECIFIED.
//  - |activation_type| for a subdocument request, if not *_UNSPECIFIED.
//  - Whether the resource |is_third_party| w.r.t. its embedding document.
bool DoesRuleFlagsMatch(const flat::UrlRule& rule,
                        proto::ElementType element_type,
                        proto::ActivationType activation_type,
                        bool is_third_party) {
  DCHECK(element_type == proto::ELEMENT_TYPE_UNSPECIFIED ||
         activation_type == proto::ACTIVATION_TYPE_UNSPECIFIED);

  if (element_type != proto::ELEMENT_TYPE_UNSPECIFIED &&
      !(rule.element_types() & element_type)) {
    return false;
  }
  if (activation_type != proto::ACTIVATION_TYPE_UNSPECIFIED &&
      !(rule.activation_types() & activation_type)) {
    return false;
  }

  if (is_third_party &&
      !(rule.options() & flat::OptionFlag_APPLIES_TO_THIRD_PARTY)) {
    return false;
  }
  if (!is_third_party &&
      !(rule.options() & flat::OptionFlag_APPLIES_TO_FIRST_PARTY)) {
    return false;
  }

  return true;
}

bool MatchesAny(const FlatUrlRuleList* rules,
                const GURL& url,
                const url::Origin& document_origin,
                proto::ElementType element_type,
                proto::ActivationType activation_type,
                bool is_third_party,
                bool disable_generic_rules) {
  if (!rules)
    return false;
  for (const flat::UrlRule* rule : *rules) {
    DCHECK_NE(rule, nullptr);
    DCHECK_NE(rule->url_pattern_type(), flat::UrlPatternType_REGEXP);
    if (!DoesRuleFlagsMatch(*rule, element_type, activation_type,
                            is_third_party)) {
      continue;
    }
    if (!UrlPattern(*rule).MatchesUrl(url))
      continue;

    if (DoesOriginMatchDomainList(document_origin, *rule,
                                  disable_generic_rules)) {
      return true;
    }
  }

  return false;
}

// Returns whether the network request matches a particular part of the index.
// |is_third_party| should reflect the relation between |url| and
// |document_origin|.
bool IsMatch(const flat::UrlPatternIndex* index,
             const GURL& url,
             const url::Origin& document_origin,
             proto::ElementType element_type,
             proto::ActivationType activation_type,
             bool is_third_party,
             bool disable_generic_rules) {
  if (!index)
    return false;
  const FlatNGramIndex* hash_table = index->ngram_index();
  const flat::NGramToRules* empty_slot = index->ngram_index_empty_slot();
  DCHECK_NE(hash_table, nullptr);

  NGramHashTableProber prober;

  auto ngrams = CreateNGramExtractor<kNGramSize, uint64_t>(
      url.spec(), [](char) { return false; });
  for (uint64_t ngram : ngrams) {
    const size_t slot_index = prober.FindSlot(
        ngram, base::strict_cast<size_t>(hash_table->size()),
        [hash_table, empty_slot](NGram ngram, size_t slot_index) {
          const flat::NGramToRules* entry = hash_table->Get(slot_index);
          DCHECK_NE(entry, nullptr);
          return entry == empty_slot || entry->ngram() == ngram;
        });
    DCHECK_LT(slot_index, hash_table->size());

    const flat::NGramToRules* entry = hash_table->Get(slot_index);
    if (entry == empty_slot)
      continue;
    if (MatchesAny(entry->rule_list(), url, document_origin, element_type,
                   activation_type, is_third_party, disable_generic_rules)) {
      return true;
    }
  }

  const FlatUrlRuleList* rules = index->fallback_rules();
  return MatchesAny(rules, url, document_origin, element_type, activation_type,
                    is_third_party, disable_generic_rules);
}

}  // namespace

// static
bool IndexedRulesetMatcher::Verify(const uint8_t* buffer, size_t size) {
  flatbuffers::Verifier verifier(buffer, size);
  return flat::VerifyIndexedRulesetBuffer(verifier);
}

IndexedRulesetMatcher::IndexedRulesetMatcher(const uint8_t* buffer, size_t size)
    : root_(flat::GetIndexedRuleset(buffer)) {
  const flat::UrlPatternIndex* index = root_->blacklist_index();
  DCHECK(!index || index->n() == kNGramSize);
  index = root_->whitelist_index();
  DCHECK(!index || index->n() == kNGramSize);
}

bool IndexedRulesetMatcher::ShouldDisableFilteringForDocument(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    proto::ActivationType activation_type) const {
  if (!document_url.is_valid() ||
      activation_type == proto::ACTIVATION_TYPE_UNSPECIFIED) {
    return false;
  }
  return IsMatch(
      root_->activation_index(), document_url, parent_document_origin,
      proto::ELEMENT_TYPE_UNSPECIFIED, activation_type,
      FirstPartyOrigin::IsThirdParty(document_url, parent_document_origin),
      false);
}

bool IndexedRulesetMatcher::ShouldDisallowResourceLoad(
    const GURL& url,
    const FirstPartyOrigin& first_party,
    proto::ElementType element_type,
    bool disable_generic_rules) const {
  if (!url.is_valid() || element_type == proto::ELEMENT_TYPE_UNSPECIFIED)
    return false;
  const bool is_third_party = first_party.IsThirdParty(url);
  return IsMatch(root_->blacklist_index(), url, first_party.origin(),
                 element_type, proto::ACTIVATION_TYPE_UNSPECIFIED,
                 is_third_party, disable_generic_rules) &&
         !IsMatch(root_->whitelist_index(), url, first_party.origin(),
                  element_type, proto::ACTIVATION_TYPE_UNSPECIFIED,
                  is_third_party, disable_generic_rules);
}

}  // namespace subresource_filter
