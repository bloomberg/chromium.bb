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
    ClipboardRecentContent* clipboard_recent_content)
    : AutocompleteProvider(AutocompleteProvider::TYPE_SHORTCUTS),
      clipboard_recent_content_(clipboard_recent_content) {
  DCHECK(clipboard_recent_content_);
}

ClipboardURLProvider::~ClipboardURLProvider() {
}

void ClipboardURLProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes,
                                 bool called_due_to_focus) {
  matches_.clear();
  // Attempt to add an AutocompleteMatch only if the user has not entered
  // anything in the omnibox.
  if (input.cursor_position() == base::string16::npos) {
    GURL url;
    if (clipboard_recent_content_->GetRecentURLFromClipboard(&url)) {
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

      matches_.push_back(match);
    }
  }
  done_ = true;
}
