// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete.h"

// This class is a base class for the history autocomplete providers and
// provides functions useful to all derived classes.
class HistoryProvider : public AutocompleteProvider {
 public:
  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

 protected:
  HistoryProvider(ACProviderListener* listener,
                  Profile* profile,
                  const char* name);

  // Fixes up user URL input to make it more possible to match against.  Among
  // many other things, this takes care of the following:
  // * Prepending file:// to file URLs
  // * Converting drive letters in file URLs to uppercase
  // * Converting case-insensitive parts of URLs (like the scheme and domain)
  //   to lowercase
  // * Convert spaces to %20s
  // Note that we don't do this in AutocompleteInput's constructor, because if
  // e.g. we convert a Unicode hostname to punycode, other providers will show
  // output that surprises the user ("Search Google for xn--6ca.com").
  // Returns false if the fixup attempt resulted in an empty string (which
  // providers generally can't do anything with).
  static bool FixupUserInput(AutocompleteInput* input);

  // Trims "http:" and up to two subsequent slashes from |url|.  Returns the
  // number of characters that were trimmed.
  // NOTE: For a view-source: URL, this will trim from after "view-source:" and
  // return 0.
  static size_t TrimHttpPrefix(string16* url);

  // Returns true if inline autocompletion should be prevented. Use this instead
  // of |input.prevent_inline_autocomplete| if the input is passed through
  // FixupUserInput(). This method returns true if
  // |input.prevent_inline_autocomplete()| is true, or the input text contains
  // trailing whitespace.
  bool PreventInlineAutocomplete(const AutocompleteInput& input);

  // Finds and removes the match from the current collection of matches and
  // backing data.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
