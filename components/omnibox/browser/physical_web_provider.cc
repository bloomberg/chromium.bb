// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/physical_web_provider.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/url_formatter/url_formatter.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

namespace {

// Relevance score of the first Physical Web URL autocomplete match. This score
// is intended to be between ClipboardURLProvider and ZeroSuggestProvider.
// Subsequent matches should decrease in relevance to preserve the ordering
// in the metadata list.
static const int kPhysicalWebUrlBaseRelevance = 700;

// The maximum length of the page title's part of the overflow item's
// description. Longer titles will be truncated to this length. In a normal
// Physical Web match item (non-overflow item) we allow the omnibox display to
// truncate the title instead.
static const size_t kMaxTitleLengthInOverflow = 15;
}

// static
const size_t PhysicalWebProvider::kPhysicalWebMaxMatches = 1;

// static
PhysicalWebProvider* PhysicalWebProvider::Create(
    AutocompleteProviderClient* client,
    HistoryURLProvider* history_url_provider) {
  return new PhysicalWebProvider(client, history_url_provider);
}

void PhysicalWebProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  DCHECK(kPhysicalWebMaxMatches < kMaxMatches);

  Stop(false, false);

  done_ = false;
  matches_.clear();

  // Stop providing suggestions when the user enters text into the omnibox.
  if (!input.from_omnibox_focus()) {
    done_ = true;
    return;
  }

  // Don't provide suggestions in incognito mode.
  if (client_->IsOffTheRecord()) {
    done_ = true;
    nearby_url_count_ = 0;
    return;
  }

  physical_web::PhysicalWebDataSource* data_source =
      client_->GetPhysicalWebDataSource();
  if (!data_source) {
    done_ = true;
    nearby_url_count_ = 0;
    return;
  }

  ConstructMatches(data_source->GetMetadata().get());

  // Physical Web matches should never be default. If the omnibox input is
  // non-empty and we have at least one Physical Web match, add the current URL
  // as the default so that hitting enter after focusing the omnibox causes the
  // current page to reload. If the input field is empty, no default match is
  // required.
  if (!matches_.empty() && !input.text().empty()) {
    matches_.push_back(VerbatimMatchForURL(client_, input, input.current_url(),
                                           history_url_provider_, -1));
  }

  done_ = true;
}

void PhysicalWebProvider::Stop(bool clear_cached_results,
                               bool due_to_user_inactivity) {
  done_ = true;
}

void PhysicalWebProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  // AddProviderInfo is called for each autocomplete provider to allow
  // provider-specific diagnostic info to be added to the omnibox log entry.
  // In this case we do not append any diagnostic info and are taking advantage
  // of the fact that this method is only called when the user has accepted an
  // autocomplete suggestion.

  // When the user accepts an autocomplete suggestion, record the number of
  // nearby Physical Web URLs at the time the provider last constructed matches.
  UMA_HISTOGRAM_EXACT_LINEAR("Omnibox.SuggestionUsed.NearbyURLCount",
                             nearby_url_count_, 50);
}

PhysicalWebProvider::PhysicalWebProvider(
    AutocompleteProviderClient* client,
    HistoryURLProvider* history_url_provider)
    : AutocompleteProvider(AutocompleteProvider::TYPE_PHYSICAL_WEB),
      client_(client),
      history_url_provider_(history_url_provider) {}

PhysicalWebProvider::~PhysicalWebProvider() {
}

void PhysicalWebProvider::ConstructMatches(base::ListValue* metadata_list) {
  nearby_url_count_ = metadata_list->GetSize();
  size_t used_slots = 0;

  for (size_t i = 0; i < nearby_url_count_; ++i) {
    base::DictionaryValue* metadata_item = NULL;
    if (!metadata_list->GetDictionary(i, &metadata_item)) {
      continue;
    }

    std::string url_string;
    std::string title_string;
    if (!metadata_item->GetString(physical_web::kResolvedUrlKey, &url_string) ||
        !metadata_item->GetString(physical_web::kTitleKey, &title_string)) {
      continue;
    }
    base::string16 title =
        AutocompleteMatch::SanitizeString(base::UTF8ToUTF16(title_string));

    // Add match items with decreasing relevance to preserve the ordering in
    // the metadata list.
    int relevance = kPhysicalWebUrlBaseRelevance - used_slots;

    // Append an overflow item if creating a match for each metadata item would
    // exceed the match limit.
    const size_t remaining_slots = kPhysicalWebMaxMatches - used_slots;
    const size_t remaining_metadata = nearby_url_count_ - i;
    if ((remaining_slots == 1) && (remaining_metadata > remaining_slots)) {
      AppendOverflowItem(remaining_metadata, relevance, title);
      break;
    }

    GURL url(url_string);

    AutocompleteMatch match(this, relevance, false,
        AutocompleteMatchType::PHYSICAL_WEB);
    match.destination_url = url;

    // Physical Web results should omit http:// (but not https://) and never
    // appear bold.
    match.contents = url_formatter::FormatUrl(url,
        url_formatter::kFormatUrlOmitHTTP, net::UnescapeRule::SPACES,
        nullptr, nullptr, nullptr);
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::URL));

    match.fill_into_edit =
        AutocompleteInput::FormattedStringWithEquivalentMeaning(
            url, match.contents, client_->GetSchemeClassifier());

    match.description = title;
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));

    matches_.push_back(match);
    ++used_slots;
  }

  UMA_HISTOGRAM_EXACT_LINEAR(
      "Omnibox.PhysicalWebProviderMatches", matches_.size(), kMaxMatches);
}

void PhysicalWebProvider::AppendOverflowItem(int additional_url_count,
                                             int relevance,
                                             const base::string16& title) {
  std::string url_string = "chrome://physical-web";
  GURL url(url_string);

  AutocompleteMatch match(this, relevance, false,
      AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW);
  match.destination_url = url;

  base::string16 contents_title = gfx::TruncateString(
      title, kMaxTitleLengthInOverflow, gfx::CHARACTER_BREAK);
  if (contents_title.empty()) {
    match.contents = l10n_util::GetPluralStringFUTF16(
        IDS_PHYSICAL_WEB_OVERFLOW_EMPTY_TITLE, additional_url_count);
  } else {
    base::string16 contents_suffix = l10n_util::GetPluralStringFUTF16(
        IDS_PHYSICAL_WEB_OVERFLOW, additional_url_count - 1);
    match.contents = contents_title + base::UTF8ToUTF16(" ") + contents_suffix;
  }
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::DIM));

  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, match.contents, client_->GetSchemeClassifier());

  match.description =
      l10n_util::GetStringUTF16(IDS_PHYSICAL_WEB_OVERFLOW_DESCRIPTION);
  match.description_class.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));

  matches_.push_back(match);
}
