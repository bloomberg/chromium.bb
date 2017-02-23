// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/clipboard_url_provider.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"

ClipboardURLProvider::ClipboardURLProvider(
    AutocompleteProviderClient* client,
    HistoryURLProvider* history_url_provider,
    ClipboardRecentContent* clipboard_content)
    : AutocompleteProvider(AutocompleteProvider::TYPE_CLIPBOARD_URL),
      client_(client),
      clipboard_content_(clipboard_content),
      history_url_provider_(history_url_provider) {
  DCHECK(clipboard_content_);
}

ClipboardURLProvider::~ClipboardURLProvider() {}

void ClipboardURLProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();

  // If the user started typing, do not offer clipboard based match.
  if (!input.from_omnibox_focus())
    return;

  GURL url;
  // If the clipboard does not contain any URL, or the URL on the page is the
  // same as the URL in the clipboard, early return.
  if (!clipboard_content_->GetRecentURLFromClipboard(&url) ||
      url == input.current_url())
    return;

  DCHECK(url.is_valid());
  // If the omnibox is not empty, add a default match.
  // This match will be opened when the user presses "Enter".
  if (!input.text().empty()) {
    AutocompleteMatch verbatim_match = VerbatimMatchForURL(
        client_, input, input.current_url(), history_url_provider_, -1);
    matches_.push_back(verbatim_match);
  }

  // Add the clipboard match. The relevance is 800 to beat ZeroSuggest results.
  AutocompleteMatch match(this, 800, false, AutocompleteMatchType::CLIPBOARD);
  match.destination_url = url;
  match.contents.assign(url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitAll, net::UnescapeRule::SPACES,
      nullptr, nullptr, nullptr));
  AutocompleteMatch::ClassifyLocationInString(
      base::string16::npos, 0, match.contents.length(),
      ACMatchClassification::URL, &match.contents_class);

  match.description.assign(l10n_util::GetStringUTF16(IDS_LINK_FROM_CLIPBOARD));
  AutocompleteMatch::ClassifyLocationInString(
      base::string16::npos, 0, match.description.length(),
      ACMatchClassification::NONE, &match.description_class);

  matches_.push_back(match);
}
