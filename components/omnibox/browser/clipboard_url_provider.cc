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
#include "components/url_formatter/url_formatter.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

ClipboardURLProvider::ClipboardURLProvider(
    AutocompleteProviderClient* client,
    ClipboardRecentContent* clipboard_content)
    : AutocompleteProvider(AutocompleteProvider::TYPE_CLIPBOARD_URL),
      client_(client),
      clipboard_content_(clipboard_content) {
  DCHECK(clipboard_content_);
}

ClipboardURLProvider::~ClipboardURLProvider() {}

void ClipboardURLProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();
  if (!input.from_omnibox_focus())
    return;

  GURL url;
  if (!clipboard_content_->GetRecentURLFromClipboard(&url) ||
      url == input.current_url())
    return;

  DCHECK(url.is_valid());
  // Adds a default match. This match will be opened when the user presses "Go".
  AutocompleteMatch verbatim_match = VerbatimMatchForURL(
      client_, input.text(), input.current_page_classification(), -1);
  if (verbatim_match.destination_url.is_valid())
    matches_.push_back(verbatim_match);

  // Add a clipboard match just below the verbatim match.
  AutocompleteMatch match(this, verbatim_match.relevance - 1, false,
                          AutocompleteMatchType::CLIPBOARD);
  match.destination_url = url;
  match.contents.assign(url_formatter::FormatUrl(
      url, client_->GetAcceptLanguages(), url_formatter::kFormatUrlOmitAll,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  AutocompleteMatch::ClassifyLocationInString(
      base::string16::npos, 0, match.contents.length(),
      ACMatchClassification::URL, &match.contents_class);

  match.description.assign(l10n_util::GetStringUTF16(IDS_LINK_FROM_CLIPBOARD));
  AutocompleteMatch::ClassifyLocationInString(
      base::string16::npos, 0, match.description.length(),
      ACMatchClassification::NONE, &match.description_class);

  // At least one match must be default, so if verbatim_match was invalid,
  // the clipboard match is allowed to be default.
  match.allowed_to_be_default_match = matches_.empty();
  matches_.push_back(match);
}
