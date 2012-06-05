// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/url_prefix.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

URLPrefix::URLPrefix(const string16& prefix, size_t num_components)
    : prefix(prefix),
      num_components(num_components) {
}

// static
const URLPrefixes& URLPrefix::GetURLPrefixes() {
  CR_DEFINE_STATIC_LOCAL(URLPrefixes, prefixes, ());
  if (prefixes.empty()) {
    prefixes.push_back(URLPrefix(ASCIIToUTF16("https://www."), 2));
    prefixes.push_back(URLPrefix(ASCIIToUTF16("http://www."), 2));
    prefixes.push_back(URLPrefix(ASCIIToUTF16("ftp://ftp."), 2));
    prefixes.push_back(URLPrefix(ASCIIToUTF16("ftp://www."), 2));
    prefixes.push_back(URLPrefix(ASCIIToUTF16("https://"), 1));
    prefixes.push_back(URLPrefix(ASCIIToUTF16("http://"), 1));
    prefixes.push_back(URLPrefix(ASCIIToUTF16("ftp://"), 1));
    prefixes.push_back(URLPrefix(string16(), 0));
  }
  return prefixes;
}

// static
bool URLPrefix::IsURLPrefix(const string16& prefix) {
  const URLPrefixes& list = GetURLPrefixes();
  for (URLPrefixes::const_iterator i = list.begin(); i != list.end(); ++i)
    if (i->prefix == prefix)
      return true;
  return false;
}

// static
const URLPrefix* URLPrefix::BestURLPrefix(const string16& text,
                                          const string16& prefix_suffix) {
  const URLPrefixes& list = GetURLPrefixes();
  for (URLPrefixes::const_iterator i = list.begin(); i != list.end(); ++i)
    if (StartsWith(text, i->prefix + prefix_suffix, false))
      return &(*i);
  return NULL;
}
