// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/url_pattern_index.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/url_pattern_index/ngram_extractor.h"
#include "components/url_pattern_index/url_pattern.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace url_pattern_index {

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
  explicit UrlRuleFlatBufferConverter(const proto::UrlRule& rule)
      : rule_(rule) {
    is_convertible_ = InitializeOptions() && InitializeElementTypes() &&
                      InitializeActivationTypes() && InitializeUrlPattern() &&
                      IsMeaningful();
  }

  // Returns whether the |rule| can be converted to its FlatBuffers equivalent.
  // The conversion is not possible if the rule has attributes not supported by
  // this client version.
  bool is_convertible() const { return is_convertible_; }

  // Writes the URL |rule| to the FlatBuffer using the |builder|, and returns
  // the offset to the serialized rule.
  UrlRuleOffset SerializeConvertedRule(
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

    // Only the following activation types are supported, ignore the others.
    activation_types_ &=
        proto::ACTIVATION_TYPE_DOCUMENT | proto::ACTIVATION_TYPE_GENERICBLOCK;

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

// Helpers. --------------------------------------------------------------------

UrlRuleOffset SerializeUrlRule(const proto::UrlRule& rule,
                               flatbuffers::FlatBufferBuilder* builder) {
  DCHECK(builder);
  UrlRuleFlatBufferConverter converter(rule);
  if (!converter.is_convertible())
    return UrlRuleOffset();
  DCHECK_NE(rule.url_pattern_type(), proto::URL_PATTERN_TYPE_REGEXP);
  return converter.SerializeConvertedRule(builder);
}

// UrlPatternIndexBuilder ------------------------------------------------------

UrlPatternIndexBuilder::UrlPatternIndexBuilder(
    flatbuffers::FlatBufferBuilder* flat_builder)
    : flat_builder_(flat_builder) {
  DCHECK(flat_builder_);
}

UrlPatternIndexBuilder::~UrlPatternIndexBuilder() = default;

void UrlPatternIndexBuilder::IndexUrlRule(UrlRuleOffset offset) {
  DCHECK(offset.o);

  const auto* rule = flatbuffers::GetTemporaryPointer(*flat_builder_, offset);
  DCHECK(rule);
  NGram ngram = GetMostDistinctiveNGram(ToStringPiece(rule->url_pattern()));

  if (ngram) {
    ngram_index_[ngram].push_back(offset);
  } else {
    // TODO(pkalinnikov): Index fallback rules as well.
    fallback_rules_.push_back(offset);
  }
}

UrlPatternIndexOffset UrlPatternIndexBuilder::Finish() {
  std::vector<flatbuffers::Offset<flat::NGramToRules>> flat_hash_table(
      ngram_index_.table_size());

  flatbuffers::Offset<flat::NGramToRules> empty_slot_offset =
      flat::CreateNGramToRules(*flat_builder_);
  for (size_t i = 0, size = ngram_index_.table_size(); i != size; ++i) {
    const uint32_t entry_index = ngram_index_.hash_table()[i];
    if (entry_index >= ngram_index_.size()) {
      flat_hash_table[i] = empty_slot_offset;
      continue;
    }
    const MutableNGramIndex::EntryType& entry =
        ngram_index_.entries()[entry_index];
    auto rules_offset = flat_builder_->CreateVector(entry.second);
    flat_hash_table[i] =
        flat::CreateNGramToRules(*flat_builder_, entry.first, rules_offset);
  }
  auto ngram_index_offset = flat_builder_->CreateVector(flat_hash_table);

  auto fallback_rules_offset = flat_builder_->CreateVector(fallback_rules_);

  return flat::CreateUrlPatternIndex(*flat_builder_, kNGramSize,
                                     ngram_index_offset, empty_slot_offset,
                                     fallback_rules_offset);
}

NGram UrlPatternIndexBuilder::GetMostDistinctiveNGram(
    base::StringPiece pattern) {
  size_t min_list_size = std::numeric_limits<size_t>::max();
  NGram best_ngram = 0;

  auto ngrams = CreateNGramExtractor<kNGramSize, NGram>(
      pattern, [](char c) { return c == '*' || c == '^'; });

  for (uint64_t ngram : ngrams) {
    const MutableUrlRuleList* rules = ngram_index_.Get(ngram);
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

// UrlPatternIndex -------------------------------------------------------------

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
  DCHECK((element_type == proto::ELEMENT_TYPE_UNSPECIFIED) !=
         (activation_type == proto::ACTIVATION_TYPE_UNSPECIFIED));

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

const flat::UrlRule* FindMatchAmongCandidates(
    const FlatUrlRuleList* candidates,
    const GURL& url,
    const url::Origin& document_origin,
    proto::ElementType element_type,
    proto::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules) {
  if (!candidates)
    return nullptr;
  for (const flat::UrlRule* rule : *candidates) {
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
      return rule;
    }
  }

  return nullptr;
}

// Returns whether the network request matches a UrlPattern |index| represented
// in its FlatBuffers format. |is_third_party| should reflect the relation
// between |url| and |document_origin|.
const flat::UrlRule* FindMatchInFlatUrlPatternIndex(
    const flat::UrlPatternIndex& index,
    const GURL& url,
    const url::Origin& document_origin,
    proto::ElementType element_type,
    proto::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules) {
  const FlatNGramIndex* hash_table = index.ngram_index();
  const flat::NGramToRules* empty_slot = index.ngram_index_empty_slot();
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
    const flat::UrlRule* rule = FindMatchAmongCandidates(
        entry->rule_list(), url, document_origin, element_type, activation_type,
        is_third_party, disable_generic_rules);
    if (rule)
      return rule;
  }

  const FlatUrlRuleList* rules = index.fallback_rules();
  return FindMatchAmongCandidates(rules, url, document_origin, element_type,
                                  activation_type, is_third_party,
                                  disable_generic_rules);
}

}  // namespace

UrlPatternIndexMatcher::UrlPatternIndexMatcher(
    const flat::UrlPatternIndex* flat_index)
    : flat_index_(flat_index) {
  DCHECK(!flat_index || flat_index->n() == kNGramSize);
}

UrlPatternIndexMatcher::~UrlPatternIndexMatcher() = default;

const flat::UrlRule* UrlPatternIndexMatcher::FindMatch(
    const GURL& url,
    const url::Origin& first_party_origin,
    proto::ElementType element_type,
    proto::ActivationType activation_type,
    bool is_third_party,
    bool disable_generic_rules) const {
  if (!flat_index_ || !url.is_valid())
    return nullptr;
  if ((element_type == proto::ELEMENT_TYPE_UNSPECIFIED) ==
      (activation_type == proto::ACTIVATION_TYPE_UNSPECIFIED)) {
    return nullptr;
  }

  return FindMatchInFlatUrlPatternIndex(*flat_index_, url, first_party_origin,
                                        element_type, activation_type,
                                        is_third_party, disable_generic_rules);
}

}  // namespace url_pattern_index
