// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_url_provider.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Score defining the priority of the suggestion. 1600 guarantees that the
// suggestion will always be first. See table in autocomplete_provider.h.
const int kClipboardURLProviderRelevance = 1600;
}  // namespace

ClipboardURLProvider::ClipboardURLProvider(
    ClipboardRecentContent* clipboard_recent_content,
    const PlaceholderRequestCallback& placeholder_match_getter)
    : AutocompleteProvider(AutocompleteProvider::TYPE_SHORTCUTS),
      clipboard_recent_content_(clipboard_recent_content),
      placeholder_match_getter_(placeholder_match_getter) {
  DCHECK(clipboard_recent_content_);
}

ClipboardURLProvider::~ClipboardURLProvider() {
}

void ClipboardURLProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();
  if (!input.from_omnibox_focus())
    return;
  // Attempt to add an AutocompleteMatch only if the user has not entered
  // anything in the omnibox.
  if (input.cursor_position() == base::string16::npos) {
    GURL url;
    // Add the suggestion only if the user is not already on the page stored
    // in the clipboard.
    if (clipboard_recent_content_->GetRecentURLFromClipboard(&url) &&
        url != input.current_url()) {
      DCHECK(url.is_valid());
      AutocompleteMatch match(this, kClipboardURLProviderRelevance, false,
                              AutocompleteMatchType::NAVSUGGEST_PERSONALIZED);
      match.allowed_to_be_default_match = true;
      match.destination_url = url;
      match.contents.assign(base::UTF8ToUTF16(url.spec()));
      AutocompleteMatch::ClassifyLocationInString(
          base::string16::npos, 0, match.contents.length(),
          ACMatchClassification::URL, &match.contents_class);

      match.description.assign(
          l10n_util::GetStringUTF16(IDS_LINK_FROM_CLIPBOARD));
      AutocompleteMatch::ClassifyLocationInString(
          base::string16::npos, 0, match.description.length(),
          ACMatchClassification::URL, &match.description_class);
      // Adds a default match. This match will be opened when the user presses
      // "Go".
      AutocompleteMatch current_url_match(placeholder_match_getter_.Run(input));
      if (current_url_match.destination_url.is_valid()) {
        // Makes the placeholder match be above the clipboard match.
        current_url_match.relevance = kClipboardURLProviderRelevance + 1;
        matches_.push_back(current_url_match);
      }

      matches_.push_back(match);
    }
  }
  done_ = true;
}
