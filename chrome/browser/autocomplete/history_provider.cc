// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_provider.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/autocomplete/autocomplete_input.h"
#include "url/url_util.h"

void HistoryProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(done_);
  DCHECK(profile_);
  DCHECK(match.deletable);

  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);

  // Delete the underlying URL along with all its visits from the history DB.
  // The resulting HISTORY_URLS_DELETED notification will also cause all caches
  // and indices to drop any data they might have stored pertaining to the URL.
  DCHECK(history_service);
  DCHECK(match.destination_url.is_valid());
  history_service->DeleteURL(match.destination_url);

  DeleteMatchFromMatches(match);
}

// static
bool HistoryProvider::PreventInlineAutocomplete(
    const AutocompleteInput& input) {
  return input.prevent_inline_autocomplete() ||
      (!input.text().empty() &&
       IsWhitespace(input.text()[input.text().length() - 1]));
}

HistoryProvider::HistoryProvider(Profile* profile,
                                 AutocompleteProvider::Type type)
    : AutocompleteProvider(type),
      profile_(profile) {
}

HistoryProvider::~HistoryProvider() {}

void HistoryProvider::DeleteMatchFromMatches(const AutocompleteMatch& match) {
  bool found = false;
  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i) {
    if (i->destination_url == match.destination_url && i->type == match.type) {
      found = true;
      if (i->is_history_what_you_typed_match || i->starred) {
        // We can't get rid of What-You-Typed or Bookmarked matches,
        // but we can make them look like they have no backing data.
        i->deletable = false;
        i->description.clear();
        i->description_class.clear();
      } else {
        matches_.erase(i);
      }
      break;
    }
  }
  DCHECK(found) << "Asked to delete a URL that isn't in our set of matches";
}

// static
ACMatchClassifications HistoryProvider::SpansFromTermMatch(
    const history::TermMatches& matches,
    size_t text_length,
    bool is_url) {
  ACMatchClassification::Style url_style =
      is_url ? ACMatchClassification::URL : ACMatchClassification::NONE;
  ACMatchClassifications spans;
  if (matches.empty()) {
    if (text_length)
      spans.push_back(ACMatchClassification(0, url_style));
    return spans;
  }
  if (matches[0].offset)
    spans.push_back(ACMatchClassification(0, url_style));
  size_t match_count = matches.size();
  for (size_t i = 0; i < match_count;) {
    size_t offset = matches[i].offset;
    spans.push_back(ACMatchClassification(offset,
        ACMatchClassification::MATCH | url_style));
    // Skip all adjacent matches.
    do {
      offset += matches[i].length;
      ++i;
    } while ((i < match_count) && (offset == matches[i].offset));
    if (offset < text_length)
      spans.push_back(ACMatchClassification(offset, url_style));
  }

  return spans;
}
