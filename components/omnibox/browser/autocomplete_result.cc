// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/l10n/l10n_util.h"

using metrics::OmniboxEventProto;

typedef AutocompleteMatchType ACMatchType;

struct MatchGURLHash {
  // The |bool| is whether the match is a calculator suggestion. We want them
  // compare differently against other matches with the same URL.
  size_t operator()(const std::pair<GURL, bool>& p) const {
    return std::hash<std::string>()(p.first.spec()) + p.second;
  }
};

// static
size_t AutocompleteResult::GetMaxMatches(bool is_zero_suggest) {
#if (defined(OS_ANDROID))
  constexpr size_t kDefaultMaxAutocompleteMatches = 5;
  if (is_zero_suggest)
    return kDefaultMaxAutocompleteMatches;
#else
  constexpr size_t kDefaultMaxAutocompleteMatches = 6;
#endif  // defined(OS_ANDROID)

  return base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam,
      kDefaultMaxAutocompleteMatches);
}

AutocompleteResult::AutocompleteResult() {
  // Reserve space for the max number of matches we'll show.
  matches_.reserve(GetMaxMatches());

  // It's probably safe to do this in the initializer list, but there's little
  // penalty to doing it here and it ensures our object is fully constructed
  // before calling member functions.
  default_match_ = end();
}

AutocompleteResult::~AutocompleteResult() {}

void AutocompleteResult::CopyOldMatches(
    const AutocompleteInput& input,
    AutocompleteResult* old_matches,
    TemplateURLService* template_url_service) {
  if (old_matches->empty())
    return;

  if (empty()) {
    // If we've got no matches we can copy everything from the last result.
    Swap(old_matches);
    for (auto i(begin()); i != end(); ++i)
      i->from_previous = true;
    return;
  }

  // In hopes of providing a stable popup we try to keep the number of matches
  // per provider consistent. Other schemes (such as blindly copying the most
  // relevant matches) typically result in many successive 'What You Typed'
  // results filling all the matches, which looks awful.
  //
  // Instead of starting with the current matches and then adding old matches
  // until we hit our overall limit, we copy enough old matches so that each
  // provider has at least as many as before, and then use SortAndCull() to
  // clamp globally. This way, old high-relevance matches will starve new
  // low-relevance matches, under the assumption that the new matches will
  // ultimately be similar.  If the assumption holds, this prevents seeing the
  // new low-relevance match appear and then quickly get pushed off the bottom;
  // if it doesn't, then once the providers are done and we expire the old
  // matches, the new ones will all become visible, so we won't have lost
  // anything permanently.
  //
  // Note that culling tail suggestions (see |MaybeCullTailSuggestions()|)
  // relies on the behavior below of capping the total number of suggestions to
  // the higher of the number of new and old suggestions.  Without it, a
  // provider could have one old and one new suggestion, cull tail suggestions,
  // expire the old suggestion, and restore tail suggestions.  This would be
  // visually unappealing, and could occur on each keystroke.
  ProviderToMatches matches_per_provider, old_matches_per_provider;
  BuildProviderToMatchesCopy(&matches_per_provider);
  // |old_matches| is going away soon, so we can move out the matches.
  old_matches->BuildProviderToMatchesMove(&old_matches_per_provider);
  for (ProviderToMatches::iterator i = old_matches_per_provider.begin();
       i != old_matches_per_provider.end(); ++i) {
    MergeMatchesByProvider(input.current_page_classification(), &i->second,
                           matches_per_provider[i->first]);
  }

  SortAndCull(input, template_url_service);
}

void AutocompleteResult::AppendMatches(const AutocompleteInput& input,
                                       const ACMatches& matches) {
  for (const auto& i : matches) {
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i.contents), i.contents);
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i.description),
              i.description);
    matches_.push_back(i);
    if (!AutocompleteMatch::IsSearchType(i.type) &&
        i.type != ACMatchType::DOCUMENT_SUGGESTION) {
      const OmniboxFieldTrial::EmphasizeTitlesCondition condition(
          OmniboxFieldTrial::GetEmphasizeTitlesConditionForInput(input));
      bool emphasize = false;
      switch (condition) {
        case OmniboxFieldTrial::EMPHASIZE_WHEN_NONEMPTY:
          emphasize = !i.description.empty();
          break;
        case OmniboxFieldTrial::EMPHASIZE_WHEN_TITLE_MATCHES:
          emphasize = !i.description.empty() &&
              AutocompleteMatch::HasMatchStyle(i.description_class);
          break;
        case OmniboxFieldTrial::EMPHASIZE_WHEN_ONLY_TITLE_MATCHES:
          emphasize = !i.description.empty() &&
              AutocompleteMatch::HasMatchStyle(i.description_class) &&
              !AutocompleteMatch::HasMatchStyle(i.contents_class);
          break;
        case OmniboxFieldTrial::EMPHASIZE_NEVER:
          break;
        default:
          NOTREACHED();
      }
      matches_.back().swap_contents_and_description = emphasize;
    }
  }
  default_match_ = end();
  alternate_nav_url_ = GURL();
}

void AutocompleteResult::SortAndCull(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service) {
  for (auto i(matches_.begin()); i != matches_.end(); ++i)
    i->ComputeStrippedDestinationURL(input, template_url_service);

#if !(defined(OS_ANDROID) || defined(OS_IOS))
  // Do not cull the tail suggestions for zero prefix query suggetions of
  // chromeOS launcher or NTP, since there won't be any default match in this
  // scenario.
  if (!(input.text().empty() &&
        (input.current_page_classification() ==
             metrics::OmniboxEventProto::CHROMEOS_APP_LIST ||
         BaseSearchProvider::IsNTPPage(input.current_page_classification())))) {
    // Wipe tail suggestions if not exclusive (minus default match).
    MaybeCullTailSuggestions(&matches_);
  }
#endif
  DemoteOnDeviceSearchSuggestions();

  DeduplicateMatches(input.current_page_classification(), &matches_);

  // Sort and trim to the most relevant GetMaxMatches() matches.
  CompareWithDemoteByType<AutocompleteMatch> comparing_object(
      input.current_page_classification());
  std::sort(matches_.begin(), matches_.end(), comparing_object);
  // Top match is not allowed to be the default match.  Find the most
  // relevant legal match and shift it to the front.
  auto it = FindTopMatch(input, &matches_);
  if (it != matches_.end()) {
    const size_t cookie = it->subrelevance;
    auto next = std::next(it);
    if (cookie != 0) {
      // If default match followed by sub-match(es), move them too.
      while (next != matches_.end() &&
             AutocompleteMatch::IsSameFamily(cookie, next->subrelevance))
        next = std::next(next);
    }
    std::rotate(matches_.begin(), it, next);
  }

  DiscourageTopMatchFromBeingSearchEntity(&matches_);

  size_t max_url_count = 0;
  if (OmniboxFieldTrial::IsMaxURLMatchesFeatureEnabled() &&
      (max_url_count = OmniboxFieldTrial::GetMaxURLMatches()) != 0)
    LimitNumberOfURLsShown(max_url_count, comparing_object);

  // In the process of trimming, drop all matches with a demoted relevance
  // score of 0.
  const size_t max_num_matches =
      std::min(GetMaxMatches(input.from_omnibox_focus()), matches_.size());
  size_t num_matches;
  for (num_matches = 0u; (num_matches < max_num_matches) &&
       (comparing_object.GetDemotedRelevance(*match_at(num_matches)) > 0);
       ++num_matches) {}
  matches_.resize(num_matches);

  if (OmniboxFieldTrial::IsGroupSuggestionsBySearchVsUrlFeatureEnabled() &&
      matches_.size() > 2) {
    // Skip over default match.
    auto next = std::next(matches_.begin());
    // If it has submatches, skip them too.
    if (matches_.front().subrelevance != 0) {
      while (next != matches_.end() &&
             AutocompleteMatch::IsSameFamily(matches_.front().subrelevance,
                                             next->subrelevance))
        next = std::next(next);
    }
    GroupSuggestionsBySearchVsURL(next, matches_.end());
  }

  // There is no default match for chromeOS launcher zero prefix query
  // suggestions.
  if (input.text().empty() && (input.current_page_classification() ==
                               metrics::OmniboxEventProto::CHROMEOS_APP_LIST)) {
    default_match_ = end();
    alternate_nav_url_ = GURL();
    return;
  }

  default_match_ = matches_.begin();

  if (default_match_ != matches_.end()) {
    const base::string16 debug_info =
        base::ASCIIToUTF16("fill_into_edit=") + default_match_->fill_into_edit +
        base::ASCIIToUTF16(", provider=") +
        ((default_match_->provider != nullptr)
             ? base::ASCIIToUTF16(default_match_->provider->GetName())
             : base::string16()) +
        base::ASCIIToUTF16(", input=") + input.text();

    // We should only get here with an empty omnibox for automatic suggestions
    // on focus on the NTP; in these cases hitting enter should do nothing, so
    // there should be no default match.  Otherwise, we're doing automatic
    // suggestions for the currently visible URL (and hitting enter should
    // reload it), or the user is typing; in either of these cases, there should
    // be a default match.
    DCHECK_NE(input.text().empty(), default_match_->allowed_to_be_default_match)
        << debug_info;

    // For navigable default matches, make sure the destination type is what the
    // user would expect given the input.
    if (default_match_->allowed_to_be_default_match &&
        default_match_->destination_url.is_valid()) {
      if (AutocompleteMatch::IsSearchType(default_match_->type)) {
        // We shouldn't get query matches for URL inputs.
        DCHECK_NE(metrics::OmniboxInputType::URL, input.type()) << debug_info;
      } else {
        // If the user explicitly typed a scheme, the default match should
        // have the same scheme.
        if ((input.type() == metrics::OmniboxInputType::URL) &&
            input.parts().scheme.is_nonempty()) {
          const std::string& in_scheme = base::UTF16ToUTF8(input.scheme());
          const std::string& dest_scheme =
              default_match_->destination_url.scheme();
          DCHECK(url_formatter::IsEquivalentScheme(in_scheme, dest_scheme))
              << debug_info;
        }
      }
    }
  }

  // Set the alternate nav URL.
  alternate_nav_url_ = (default_match_ == matches_.end()) ?
      GURL() : ComputeAlternateNavUrl(input, *default_match_);
}

void AutocompleteResult::DemoteOnDeviceSearchSuggestions() {
  const std::string mode = base::GetFieldTrialParamValueByFeature(
      omnibox::kOnDeviceHeadProvider, "DemoteOnDeviceSearchSuggestionsMode");
  if (mode != "decrease-relevances" && mode != "remove-suggestions")
    return;

  std::vector<AutocompleteMatch*> on_device_search_suggestions;
  int search_provider_search_suggestion_min_relevance = -1,
      on_device_search_suggestion_max_relevance = -1;
  bool search_provider_search_suggestion_exists = false;

  // Loop through all matches to check the existence of SearchProvider search
  // suggestions and OnDeviceProvider search suggestions. Also calculate the
  // maximum OnDeviceProvider search suggestion relevance and the minimum
  // SearchProvider search suggestion relevance, in preparation to adjust the
  // relevances for OnDeviceProvider search suggestions next.
  for (auto& m : matches_) {
    // The demotion will not be triggered if only trivial suggestions present,
    // which include type SEARCH_WHAT_YOU_TYPED & SEARCH_OTHER_ENGINE.
    // Note that we exclude SEARCH_OTHER_ENGINE here, simply because custom
    // search engine ("keyword search") is not enabled at Android & iOS, where
    // on device suggestion providers will be enabled. We should revisit this
    // triggering condition once keyword search is launched at Android & iOS.
    if (m.IsSearchProviderSearchSuggestion() && !m.IsTrivialAutocompletion()) {
      search_provider_search_suggestion_exists = true;
      if (mode == "decrease-relevances") {
        search_provider_search_suggestion_min_relevance =
            search_provider_search_suggestion_min_relevance < 0
                ? m.relevance
                : std::min(search_provider_search_suggestion_min_relevance,
                           m.relevance);
      }
    } else if (m.IsOnDeviceSearchSuggestion()) {
      on_device_search_suggestions.push_back(&m);
      if (mode == "decrease-relevances") {
        on_device_search_suggestion_max_relevance =
            std::max(on_device_search_suggestion_max_relevance, m.relevance);
      }
    }
  }

  // If SearchProvider search suggestions present, adjust the relevances for
  // OnDeviceProvider search suggestions, determined by mode:
  // 1. decrease-relevances: if any OnDeviceProvider search suggestion has a
  //    higher relevance than any SearchProvider one, subtract the difference
  //    b/w the maximum OnDeviceProvider search suggestion relevance and the
  //    minimum SearchProvider search suggestion relevance from the relevances
  //    for all OnDeviceProvider ones.
  // 2. remove-suggestions: set the relevances to 0 for all OnDeviceProvider
  //    search suggestions.
  if (search_provider_search_suggestion_exists &&
      !on_device_search_suggestions.empty()) {
    if (mode == "decrease-relevances" &&
        on_device_search_suggestion_max_relevance >=
            search_provider_search_suggestion_min_relevance) {
      int relevance_offset =
          (on_device_search_suggestion_max_relevance -
           search_provider_search_suggestion_min_relevance + 1);
      for (auto* m : on_device_search_suggestions)
        m->relevance = m->relevance > relevance_offset
                           ? m->relevance - relevance_offset
                           : 0;
    } else if (mode == "remove-suggestions") {
      for (auto* m : on_device_search_suggestions)
        m->relevance = 0;
    }
  }
}

void AutocompleteResult::AppendDedicatedPedalMatches(
    AutocompleteProviderClient* client,
    const AutocompleteInput& input) {
  const OmniboxPedalProvider* provider = client->GetPedalProvider();
  ACMatches pedal_suggestions;

  for (auto& match : matches_) {
    // We do not want to deal with pedals of pedals, or pedals among
    // exclusive tail suggestions.
    if (match.pedal || match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL)
      continue;
    OmniboxPedal* const pedal = provider->FindPedalMatch(match.contents);
    if (pedal)
      pedal_suggestions.push_back(match.DerivePedalSuggestion(pedal));
  }
  if (!pedal_suggestions.empty()) {
    AppendMatches(input, pedal_suggestions);
  }
}

void AutocompleteResult::ConvertOpenTabMatches(
    AutocompleteProviderClient* client,
    const AutocompleteInput* input) {
  ACMatches matches_to_add;
  for (auto& match : matches_) {
    // If already converted this match, don't re-search through open tabs and
    // possibly re-change the description. Also skip submatches.
    if (match.has_tab_match || match.IsSubMatch())
      continue;
    // If URL is in a tab, remember that.
    if (client->IsTabOpenWithURL(match.destination_url, input)) {
      match.has_tab_match = true;
      // If will have dedicated row, add a match for it.
      if (OmniboxFieldTrial::IsTabSwitchSuggestionsDedicatedRowEnabled()) {
        if (match.subrelevance == 0)
          match.subrelevance = AutocompleteMatch::GetNextFamilyID();
        AutocompleteMatch tab_switch_match = match;
        tab_switch_match.SetSubMatch(
            match.subrelevance + AutocompleteMatch::TAB_SWITCH_FAMILY_ID,
            match.type);
        tab_switch_match.contents =
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT);
        tab_switch_match.contents_class = {{0, ACMatchClassification::NONE}};
        matches_to_add.push_back(tab_switch_match);
      }
    }
  }
  std::copy(matches_to_add.begin(), matches_to_add.end(),
            std::back_inserter(matches_));
}

bool AutocompleteResult::HasCopiedMatches() const {
  for (auto i(begin()); i != end(); ++i) {
    if (i->from_previous)
      return true;
  }
  return false;
}

size_t AutocompleteResult::size() const {
  return matches_.size();
}

bool AutocompleteResult::empty() const {
  return matches_.empty();
}

AutocompleteResult::const_iterator AutocompleteResult::begin() const {
  return matches_.begin();
}

AutocompleteResult::iterator AutocompleteResult::begin() {
  return matches_.begin();
}

AutocompleteResult::const_iterator AutocompleteResult::end() const {
  return matches_.end();
}

AutocompleteResult::iterator AutocompleteResult::end() {
  return matches_.end();
}

// Returns the match at the given index.
const AutocompleteMatch& AutocompleteResult::match_at(size_t index) const {
  DCHECK_LT(index, matches_.size());
  return matches_[index];
}

AutocompleteMatch* AutocompleteResult::match_at(size_t index) {
  DCHECK_LT(index, matches_.size());
  return &matches_[index];
}

bool AutocompleteResult::TopMatchIsStandaloneVerbatimMatch() const {
  if (empty() || !match_at(0).IsVerbatimType())
    return false;

  // Skip any copied matches, under the assumption that they'll be expired and
  // disappear.  We don't want this disappearance to cause the visibility of the
  // top match to change.
  for (auto i(begin() + 1); i != end(); ++i) {
    if (!i->from_previous)
      return !i->IsVerbatimType();
  }
  return true;
}

// static
ACMatches::const_iterator AutocompleteResult::FindTopMatch(
    const AutocompleteInput& input,
    const ACMatches& matches) {
  return FindTopMatch(input, const_cast<ACMatches*>(&matches));
}

// static
ACMatches::iterator AutocompleteResult::FindTopMatch(
    const AutocompleteInput& input,
    ACMatches* matches) {
  // The matches may be sorted by type-demoted relevance. If we want to choose
  // the highest-relevance, allowed-to-be-default match while ignoring type
  // demotion, as we do when IsPreserveDefaultMatchScoreEnabled is true, we need
  // to explicitly find the highest relevance match rather than just accepting
  // the first allowed-to-be--default match in the list.
  // The goal of this behavior is to ensure that in situations where the user
  // expects to see a commonly visited URL as the default match, the URL is not
  // suppressed by type demotion.
  // However, even if IsPreserveDefaultMatchScoreEnabled is true, we don't care
  // about this URL behavior when the user is using the fakebox, which is
  // intended to work more like a search-only box. Unless the user's input is a
  // URL in which case we still want to ensure they can get a URL as the default
  // match.
  if ((input.current_page_classification() !=
           OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS ||
       input.type() == metrics::OmniboxInputType::URL) &&
      OmniboxFieldTrial::IsPreserveDefaultMatchScoreEnabled()) {
    auto best = matches->end();
    for (auto it = matches->begin(); it != matches->end(); ++it) {
      if (it->allowed_to_be_default_match && !it->IsSubMatch() &&
          (best == matches->end() ||
           AutocompleteMatch::MoreRelevant(*it, *best))) {
        best = it;
      }
    }
    return best;
  } else {
    return std::find_if(matches->begin(), matches->end(), [](const auto& m) {
      return m.allowed_to_be_default_match;
    });
  }
}

// static
void AutocompleteResult::DiscourageTopMatchFromBeingSearchEntity(
    ACMatches* matches) {
  if (matches->empty())
    return;

  auto top_match = matches->begin();
  if (top_match->type != ACMatchType::SEARCH_SUGGEST_ENTITY)
    return;

  // Search the duplicates for a equivalent non-entity search suggestion.
  for (auto it = top_match->duplicate_matches.begin();
       it != top_match->duplicate_matches.end(); ++it) {
    // Reject any ineligible duplicates.
    if (it->type == ACMatchType::SEARCH_SUGGEST_ENTITY ||
        !AutocompleteMatch::IsSearchType(it->type) ||
        !it->allowed_to_be_default_match) {
      continue;
    }

    // Copy the non-entity match, then erase it from the list of duplicates.
    // We do this first, because the insertion operation invalidates all
    // iterators, including |top_match|.
    AutocompleteMatch non_entity_match_copy = *it;
    top_match->duplicate_matches.erase(it);

    // Promote the non-entity match to the top, then immediately return, since
    // all our iterators are invalid after the insertion.
    matches->insert(matches->begin(), std::move(non_entity_match_copy));
    return;
  }
}

void AutocompleteResult::Reset() {
  matches_.clear();
  default_match_ = end();
}

void AutocompleteResult::Swap(AutocompleteResult* other) {
  const size_t default_match_offset = default_match_ - begin();
  const size_t other_default_match_offset =
      other->default_match_ - other->begin();
  matches_.swap(other->matches_);
  default_match_ = begin() + other_default_match_offset;
  other->default_match_ = other->begin() + default_match_offset;
  alternate_nav_url_.Swap(&(other->alternate_nav_url_));
}

void AutocompleteResult::CopyFrom(const AutocompleteResult& rhs) {
  if (this == &rhs)
    return;

  matches_ = rhs.matches_;
  // Careful!  You can't just copy iterators from another container, you have to
  // reconstruct them.
  default_match_ = (rhs.default_match_ == rhs.end())
                       ? end()
                       : (begin() + (rhs.default_match_ - rhs.begin()));

  alternate_nav_url_ = rhs.alternate_nav_url_;
}

#if DCHECK_IS_ON()
void AutocompleteResult::Validate() const {
  for (auto i(begin()); i != end(); ++i)
    i->Validate();
}
#endif  // DCHECK_IS_ON()

// static
GURL AutocompleteResult::ComputeAlternateNavUrl(
    const AutocompleteInput& input,
    const AutocompleteMatch& match) {
  return ((input.type() == metrics::OmniboxInputType::UNKNOWN) &&
          (AutocompleteMatch::IsSearchType(match.type)) &&
          !ui::PageTransitionCoreTypeIs(match.transition,
                                        ui::PAGE_TRANSITION_KEYWORD) &&
          (input.canonicalized_url() != match.destination_url))
             ? input.canonicalized_url()
             : GURL();
}

void AutocompleteResult::DeduplicateMatches(
    metrics::OmniboxEventProto::PageClassification page_classification,
    ACMatches* matches) {
  // Group matches by stripped URL and whether it's a calculator suggestion.
  std::unordered_map<std::pair<GURL, bool>, std::vector<ACMatches::iterator>,
                     MatchGURLHash>
      url_to_matches;
  for (auto i = matches->begin(); i != matches->end(); ++i) {
    std::pair<GURL, bool> p = GetMatchComparisonFields(*i);
    url_to_matches[p].push_back(i);
  }

  // For each group of duplicate matches, choose the one that's considered best.
  for (auto& group : url_to_matches) {
    const auto& key = group.first;
    const GURL& gurl = key.first;
    // The vector of matches whose URL are equivalent.
    std::vector<ACMatches::iterator>& duplicate_matches = group.second;
    if (gurl.is_empty() || duplicate_matches.size() == 1)
      continue;

    // Sort the matches best to worst, according to the deduplication criteria.
    std::sort(duplicate_matches.begin(), duplicate_matches.end(),
              &AutocompleteMatch::BetterDuplicateByIterator);
    AutocompleteMatch& best_match = **duplicate_matches.begin();

    // Process all the duplicate matches (from second-best to worst).
    std::vector<AutocompleteMatch> duplicates_of_duplicates;
    for (auto i = std::next(duplicate_matches.begin());
         i != duplicate_matches.end(); ++i) {
      AutocompleteMatch& duplicate_match = **i;

      // Each duplicate match may also have its own duplicates. Move those to
      // a temporary list, which will be eventually added to the end of
      // |best_match.duplicate_matches|. Clear out the original list too.
      std::move(duplicate_match.duplicate_matches.begin(),
                duplicate_match.duplicate_matches.end(),
                std::back_inserter(duplicates_of_duplicates));
      duplicate_match.duplicate_matches.clear();

      best_match.UpgradeMatchWithPropertiesFrom(duplicate_match);

      // This should be a copy, not a move, since we don't erase duplicate
      // matches from the master list until the very end.
      DCHECK(duplicate_match.duplicate_matches.empty());  // Should be cleared.
      best_match.duplicate_matches.push_back(duplicate_match);
    }
    std::move(duplicates_of_duplicates.begin(), duplicates_of_duplicates.end(),
              std::back_inserter(best_match.duplicate_matches));
  }

  // Erase duplicate matches.
  base::EraseIf(*matches, [&url_to_matches](const AutocompleteMatch& m) {
    std::pair<GURL, bool> p = GetMatchComparisonFields(m);
    return !m.stripped_destination_url.is_empty() &&
           &(*url_to_matches[p].front()) != &m;
  });
}

void AutocompleteResult::InlineTailPrefixes() {
  base::string16 common_prefix;

  for (const auto& match : matches_) {
    if (match.type == ACMatchType::SEARCH_SUGGEST_TAIL) {
      int common_length;
      base::StringToInt(
          match.GetAdditionalInfo(kACMatchPropertyContentsStartIndex),
          &common_length);
      common_prefix = base::UTF8ToUTF16(match.GetAdditionalInfo(
                                            kACMatchPropertySuggestionText))
                          .substr(0, common_length);
      break;
    }
  }
  if (common_prefix.size()) {
    for (auto& match : matches_)
      match.InlineTailPrefix(common_prefix);
  }
}

size_t AutocompleteResult::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(matches_);
  res += base::trace_event::EstimateMemoryUsage(alternate_nav_url_);

  return res;
}

// static
bool AutocompleteResult::HasMatchByDestination(const AutocompleteMatch& match,
                                               const ACMatches& matches) {
  for (auto i(matches.begin()); i != matches.end(); ++i) {
    if (i->destination_url == match.destination_url)
      return true;
  }
  return false;
}

// static
void AutocompleteResult::MaybeCullTailSuggestions(ACMatches* matches) {
  std::function<bool(const AutocompleteMatch&)> is_tail =
      [](const AutocompleteMatch& match) {
        return match.type == ACMatchType::SEARCH_SUGGEST_TAIL;
      };
  auto non_tail_default = std::find_if(
      matches->begin(), matches->end(), [&](const AutocompleteMatch& match) {
        return match.allowed_to_be_default_match && !is_tail(match);
      });
  // If the only default matches are tail suggestions, let them remain and
  // instead remove the non-tail suggestions.  This is necessary because we do
  // not want to display tail suggestions mixed with other suggestions in the
  // dropdown below the first item (the default match).  In this case, we
  // cannot remove the tail suggestions because we'll be left without a legal
  // default match--the non-tail ones much go.  This situation though is
  // unlikely, as we normally would expect the search-what-you-typed suggestion
  // as a default match (and that's a non-tail suggestion).
  if (non_tail_default == matches->end()) {
    base::EraseIf(*matches, [&is_tail](const AutocompleteMatch& match) {
      return !is_tail(match);
    });
    return;
  }
  // Determine if there are both tail and non-tail matches, excluding the
  // non-tail default match.
  bool any_tail = false, any_non_tail = false;
  for (auto i = matches->begin();
       i != matches->end() && !(any_tail && any_non_tail); ++i) {
    // We allow one default non-tail match.
    if (i != non_tail_default) {
      if (is_tail(*i))
        any_tail = true;
      else
        any_non_tail = true;
    }
  }
  // If both tail and non-tail matches, remove tail. Note that this can
  // remove the highest rated suggestions.
  if (any_tail) {
    if (any_non_tail) {
      base::EraseIf(*matches, is_tail);
    } else {
      // We want the non-tail default match to be first. Mark tail suggestions
      // as not a legal default match, so that the default match will be moved
      // up explicitly.
      for (auto& match : *matches) {
        if (is_tail(match))
          match.allowed_to_be_default_match = false;
      }
    }
  }
}

void AutocompleteResult::BuildProviderToMatchesCopy(
    ProviderToMatches* provider_to_matches) const {
  for (const auto& match : *this)
    (*provider_to_matches)[match.provider].push_back(match);
}

void AutocompleteResult::BuildProviderToMatchesMove(
    ProviderToMatches* provider_to_matches) {
  for (auto& match : *this)
    (*provider_to_matches)[match.provider].push_back(std::move(match));
}

void AutocompleteResult::MergeMatchesByProvider(
    metrics::OmniboxEventProto::PageClassification page_classification,
    ACMatches* old_matches,
    const ACMatches& new_matches) {
  if (new_matches.size() >= old_matches->size())
    return;

  // Prevent old matches from this provider from outranking new ones and
  // becoming the default match by capping old matches' scores to be less than
  // the highest-scoring allowed-to-be-default match from this provider.
  auto i = std::find_if(
      new_matches.begin(), new_matches.end(),
      [](const AutocompleteMatch& m) { return m.allowed_to_be_default_match; });

  // If the provider doesn't have any matches that are allowed-to-be-default,
  // cap scores below the global allowed-to-be-default match.
  // AutocompleteResult maintains the invariant that the first item in
  // |matches_| is always such a match.
  if (i == new_matches.end())
    i = matches_.begin();

  const int max_relevance = i->relevance - 1;

  // Because the goal is a visibly-stable popup, rather than one that preserves
  // the highest-relevance matches, we copy in the lowest-relevance matches
  // first. This means that within each provider's "group" of matches, any
  // synchronous matches (which tend to have the highest scores) will
  // "overwrite" the initial matches from that provider's previous results,
  // minimally disturbing the rest of the matches.
  size_t delta = old_matches->size() - new_matches.size();
  for (auto i = old_matches->rbegin(); i != old_matches->rend() && delta > 0;
       ++i) {
    if (!HasMatchByDestination(*i, new_matches)) {
      matches_.push_back(std::move(*i));
      matches_.back().relevance =
          std::min(max_relevance, matches_.back().relevance);
      matches_.back().from_previous = true;
      delta--;
    }
  }
}

std::pair<GURL, bool> AutocompleteResult::GetMatchComparisonFields(
    const AutocompleteMatch& match) {
  return std::make_pair(match.stripped_destination_url,
                        match.type == ACMatchType::CALCULATOR ||
                            // Separate sub-matches from their origins.
                            match.IsSubMatch());
}

void AutocompleteResult::LimitNumberOfURLsShown(
    size_t max_url_count,
    const CompareWithDemoteByType<AutocompleteMatch>& comparing_object) {
  size_t search_count = std::count_if(
      matches_.begin(), matches_.end(), [&](const AutocompleteMatch& m) {
        return !m.IsSubMatch() && AutocompleteMatch::IsSearchType(m.type) &&
               // Don't count if would be removed.
               comparing_object.GetDemotedRelevance(m) > 0;
      });
  // Display more than GetMaxURLMatches() if there are no non-URL suggestions
  // to replace them. Avoid signed math.
  if (GetMaxMatches() > search_count &&
      GetMaxMatches() - search_count > max_url_count)
    max_url_count = GetMaxMatches() - search_count;
  size_t url_count = 0;
  // Erase URL suggestions past the count of allowed ones, or anything past
  // maximum.
  matches_.erase(
      std::remove_if(matches_.begin(), matches_.end(),
                     [&url_count, max_url_count](const AutocompleteMatch& m) {
                       if (!m.IsSubMatch() &&
                           !AutocompleteMatch::IsSearchType(m.type) &&
                           ++url_count > max_url_count)
                         return true;
                       // Do not count submatches towards URL total, but
                       // drop them if parent was dropped.
                       if (m.IsSubMatch() &&
                           !AutocompleteMatch::IsSearchType(m.parent_type) &&
                           url_count > max_url_count)
                         return true;
                       return false;
                     }),
      matches_.end());
}

// static
void AutocompleteResult::GroupSuggestionsBySearchVsURL(iterator begin,
                                                       iterator end) {
  std::stable_partition(begin, end, [](const AutocompleteMatch& match) {
    return match.IsSearchType(match.GetDemotionType());
  });
}
