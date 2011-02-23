// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/builtin_provider.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/net/url_fixer_upper.h"

const int BuiltinProvider::kRelevance = 575;

BuiltinProvider::BuiltinProvider(ACProviderListener* listener,
                                 Profile* profile)
    : AutocompleteProvider(listener, profile, "Builtin") {
  std::vector<std::string> builtins(AboutPaths());
  for (std::vector<std::string>::iterator i(builtins.begin());
       i != builtins.end(); ++i)
    builtins_.push_back(ASCIIToUTF16("about:") + ASCIIToUTF16(*i));
}

BuiltinProvider::~BuiltinProvider() {}

void BuiltinProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY) ||
      (input.type() == AutocompleteInput::QUERY))
    return;
  for (Builtins::const_iterator i(builtins_.begin());
       (i != builtins_.end()) && (matches_.size() < kMaxMatches); ++i) {
    if (StartsWith(*i, input.text(), false)) {
      AutocompleteMatch match(this, kRelevance, false,
                              AutocompleteMatch::NAVSUGGEST);
      match.fill_into_edit = *i;
      match.destination_url = GURL(*i);
      match.contents = match.fill_into_edit;
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
  for (size_t i = 0; i < matches_.size(); ++i)
    matches_[i].relevance = kRelevance + matches_.size() - (i + 1);
}
