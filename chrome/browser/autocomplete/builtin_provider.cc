// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/builtin_provider.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/common/url_constants.h"

const int BuiltinProvider::kRelevance = 575;

BuiltinProvider::Builtin::Builtin(const string16& path, const GURL& url)
    : path(path), url(url) {
};

BuiltinProvider::BuiltinProvider(ACProviderListener* listener,
                                 Profile* profile)
    : AutocompleteProvider(listener, profile, "Builtin") {
  std::string about("about:");
  std::vector<std::string> paths = AboutPaths();
  for (std::vector<std::string>::const_iterator path = paths.begin();
      path != paths.end(); ++path) {
    builtins_.push_back(Builtin(UTF8ToUTF16(about + *path),
                                AboutPathToURL(*path)));
  }
}

void BuiltinProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY) ||
      (input.type() == AutocompleteInput::QUERY))
    return;
  for (std::vector<Builtin>::const_iterator builtin = builtins_.begin();
      (builtin != builtins_.end()) && (matches_.size() < kMaxMatches);
      ++builtin) {
    if (!builtin->path.compare(0, input.text().length(), input.text())) {
      AutocompleteMatch match(this, kRelevance, false,
                              AutocompleteMatch::NAVSUGGEST);
      match.fill_into_edit = builtin->path;
      match.destination_url = builtin->url;
      match.contents = builtin->path;
      match.contents_class.push_back(ACMatchClassification(0,
          ACMatchClassification::MATCH | ACMatchClassification::URL));
      if (match.contents.length() > input.text().length()) {
        match.contents_class.push_back(
            ACMatchClassification(input.text().length(),
                                  ACMatchClassification::URL));
      }
      matches_.push_back(match);
    }
  }
  for (size_t i = 0; i < matches_.size(); i++)
    matches_[i].relevance = kRelevance + matches_.size() - (i + 1);
}
