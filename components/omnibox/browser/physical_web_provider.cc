// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/physical_web_provider.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/url_formatter/url_formatter.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// The maximum number of match results to provide. If the number of nearby URLs
// exceeds this limit, an overflow item is created. Tapping the overflow item
// navigates to a page with the full list of nearby URLs. The overflow item is
// counted as a match result for the purposes of the match limit.
//
// ex: With kPhysicalWebMaxMatches == 1, there should be at most one suggestion
// created by this provider. If there is a single nearby URL, then the
// suggestion will be for that URL. If there are multiple nearby URLs, the
// suggestion will be the overflow item which navigates to the WebUI when
// tapped.
static const size_t kPhysicalWebMaxMatches = 1;

// Relevance score of the first Physical Web URL autocomplete match. This score
// is intended to be between ClipboardURLProvider and ZeroSuggestProvider.
// Subsequent matches should decrease in relevance to preserve the ordering
// in the metadata list.
static const int kPhysicalWebUrlBaseRelevance = 700;

}

// static
PhysicalWebProvider* PhysicalWebProvider::Create(
    AutocompleteProviderClient* client) {
  return new PhysicalWebProvider(client);
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

  PhysicalWebDataSource* data_source = client_->GetPhysicalWebDataSource();
  if (!data_source) {
    done_ = true;
    return;
  }

  ConstructMatches(data_source->GetMetadata().get());

  done_ = true;
}

void PhysicalWebProvider::Stop(bool clear_cached_results,
                               bool due_to_user_inactivity) {
  done_ = true;
}

PhysicalWebProvider::PhysicalWebProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_PHYSICAL_WEB),
      client_(client) {
}

PhysicalWebProvider::~PhysicalWebProvider() {
}

void PhysicalWebProvider::ConstructMatches(base::ListValue* metadata_list) {
  const size_t metadata_count = metadata_list->GetSize();

  for (size_t i = 0; i < metadata_count; ++i) {
    base::DictionaryValue* metadata_item = NULL;
    if (!metadata_list->GetDictionary(i, &metadata_item)) {
      continue;
    }

    std::string url_string;
    std::string title_string;
    if (!metadata_item->GetString("resolvedUrl", &url_string) ||
        !metadata_item->GetString("title", &title_string)) {
      continue;
    }

    // Add match items with decreasing relevance to preserve the ordering in
    // the metadata list.
    int relevance = kPhysicalWebUrlBaseRelevance - matches_.size();

    // Append an overflow item if creating a match for each metadata item would
    // exceed the match limit.
    const size_t remaining_slots = kPhysicalWebMaxMatches - matches_.size();
    const size_t remaining_metadata = metadata_count - i;
    if ((remaining_slots == 1) && (remaining_metadata > remaining_slots)) {
      AppendOverflowItem(remaining_metadata, relevance);
      return;
    }

    GURL url(url_string);
    base::string16 title = base::UTF8ToUTF16(title_string);

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

    match.description = AutocompleteMatch::SanitizeString(title);
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));

    match.allowed_to_be_default_match = matches_.empty();

    matches_.push_back(match);
  }
}

void PhysicalWebProvider::AppendOverflowItem(int additional_url_count,
                                             int relevance) {
  std::string url_string = "chrome://physical-web";
  GURL url(url_string);

  AutocompleteMatch match(this, relevance, false,
      AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW);
  match.destination_url = url;

  // Don't omit the chrome:// scheme when displaying the WebUI URL.
  match.contents = url_formatter::FormatUrl(url,
      url_formatter::kFormatUrlOmitNothing, net::UnescapeRule::SPACES,
      nullptr, nullptr, nullptr);
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));

  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, match.contents, client_->GetSchemeClassifier());

  match.description = l10n_util::GetPluralStringFUTF16(
      IDS_PHYSICAL_WEB_OVERFLOW, additional_url_count);
  match.description_class.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));

  match.allowed_to_be_default_match = matches_.empty();

  matches_.push_back(match);
}
