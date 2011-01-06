// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete.h"

namespace history {

class HistoryBackend;
class URLDatabase;
class URLRow;

}  // namespace history

// This class is a base class for the history autocomplete providers and
// provides functions useful to all derived classes.
class HistoryProvider : public AutocompleteProvider {
 public:
  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

 protected:
  enum MatchType {
    NORMAL,
    WHAT_YOU_TYPED,
    INLINE_AUTOCOMPLETE
  };

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
  static std::wstring FixupUserInput(const AutocompleteInput& input);

  // Trims "http:" and up to two subsequent slashes from |url|.  Returns the
  // number of characters that were trimmed.
  // NOTE: For a view-source: URL, this will trim from after "view-source:" and
  // return 0.
  static size_t TrimHttpPrefix(std::wstring* url);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
