// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/url_prefix.h"

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"

namespace {

// Like URLPrefix::BestURLPrefix() except also handles the prefix of
// "www.".
const URLPrefix* BestURLPrefixWithWWWCase(
    const base::string16& text,
    const base::string16& prefix_suffix) {
  CR_DEFINE_STATIC_LOCAL(URLPrefix, www_prefix,
                         (base::ASCIIToUTF16("www."), 1));
  const URLPrefix* best_prefix = URLPrefix::BestURLPrefix(text, prefix_suffix);
  if ((best_prefix == NULL) ||
      (best_prefix->num_components < www_prefix.num_components)) {
    if (URLPrefix::PrefixMatch(www_prefix, text, prefix_suffix))
      best_prefix = &www_prefix;
  }
  return best_prefix;
}

}  // namespace

URLPrefix::URLPrefix(const base::string16& prefix, size_t num_components)
    : prefix(prefix),
      num_components(num_components) {
}

// static
const URLPrefixes& URLPrefix::GetURLPrefixes() {
  CR_DEFINE_STATIC_LOCAL(URLPrefixes, prefixes, ());
  if (prefixes.empty()) {
    prefixes.push_back(URLPrefix(base::ASCIIToUTF16("https://www."), 2));
    prefixes.push_back(URLPrefix(base::ASCIIToUTF16("http://www."), 2));
    prefixes.push_back(URLPrefix(base::ASCIIToUTF16("ftp://ftp."), 2));
    prefixes.push_back(URLPrefix(base::ASCIIToUTF16("ftp://www."), 2));
    prefixes.push_back(URLPrefix(base::ASCIIToUTF16("https://"), 1));
    prefixes.push_back(URLPrefix(base::ASCIIToUTF16("http://"), 1));
    prefixes.push_back(URLPrefix(base::ASCIIToUTF16("ftp://"), 1));
    prefixes.push_back(URLPrefix(base::string16(), 0));
  }
  return prefixes;
}

// static
bool URLPrefix::IsURLPrefix(const base::string16& prefix) {
  const URLPrefixes& list = GetURLPrefixes();
  for (URLPrefixes::const_iterator i = list.begin(); i != list.end(); ++i)
    if (i->prefix == prefix)
      return true;
  return false;
}

// static
const URLPrefix* URLPrefix::BestURLPrefix(const base::string16& text,
                                          const base::string16& prefix_suffix) {
  const URLPrefixes& list = GetURLPrefixes();
  for (URLPrefixes::const_iterator i = list.begin(); i != list.end(); ++i)
    if (PrefixMatch(*i, text, prefix_suffix))
      return &(*i);
  return NULL;
}

// static
bool URLPrefix::PrefixMatch(const URLPrefix& prefix,
                            const base::string16& text,
                            const base::string16& prefix_suffix) {
  return StartsWith(text, prefix.prefix + prefix_suffix, false);
}

// static
void URLPrefix::ComputeMatchStartAndInlineAutocompleteOffset(
    const AutocompleteInput& input,
    const AutocompleteInput& fixed_up_input,
    const bool allow_www_prefix_without_scheme,
    const base::string16& text,
    size_t* match_start,
    size_t* inline_autocomplete_offset) {
  const URLPrefix* best_prefix = allow_www_prefix_without_scheme ?
      BestURLPrefixWithWWWCase(text, input.text()) :
      BestURLPrefix(text, input.text());
  const base::string16* matching_string = &input.text();
  // If we failed to find a best_prefix initially, try again using a fixed-up
  // version of the user input.  This is especially useful to get about: URLs
  // to inline against chrome:// shortcuts.  (about: URLs are fixed up to the
  // chrome:// scheme.)
  if ((best_prefix == NULL) && !fixed_up_input.text().empty() &&
      (fixed_up_input.text() != input.text())) {
    best_prefix = allow_www_prefix_without_scheme ?
        BestURLPrefixWithWWWCase(text, fixed_up_input.text()) :
        BestURLPrefix(text, fixed_up_input.text());
    matching_string = &fixed_up_input.text();
  }
  if (best_prefix != NULL) {
    *match_start = best_prefix->prefix.length();
    *inline_autocomplete_offset =
        best_prefix->prefix.length() + matching_string->length();
  } else {
    *match_start = base::string16::npos;
    *inline_autocomplete_offset = base::string16::npos;
  }
}
