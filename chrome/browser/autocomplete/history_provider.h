// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "components/omnibox/autocomplete_provider.h"

class AutocompleteInput;
class Profile;
struct AutocompleteMatch;

// This class is a base class for the history autocomplete providers and
// provides functions useful to all derived classes.
class HistoryProvider : public AutocompleteProvider {
 public:
  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

  // Returns true if inline autocompletion should be prevented for URL-like
  // input.  This method returns true if input.prevent_inline_autocomplete()
  // is true or the input text contains trailing whitespace.
  static bool PreventInlineAutocomplete(const AutocompleteInput& input);

 protected:
  HistoryProvider(Profile* profile, AutocompleteProvider::Type type);
  virtual ~HistoryProvider();

  // Finds and removes the match from the current collection of matches and
  // backing data.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);

  // Fill and return an ACMatchClassifications structure given the |matches|
  // to highlight.
  static ACMatchClassifications SpansFromTermMatch(
      const history::TermMatches& matches,
      size_t text_length,
      bool is_url);

  Profile* profile_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
