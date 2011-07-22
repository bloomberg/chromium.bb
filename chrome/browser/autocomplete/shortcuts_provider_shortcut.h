// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_SHORTCUT_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_SHORTCUT_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "googleurl/src/gurl.h"

namespace shortcuts_provider {

// The following struct encapsulates one previously selected omnibox shortcut.
struct Shortcut {
  Shortcut(const string16& text,
           const GURL& url,
           const string16& contents,
           const ACMatchClassifications& contents_class,
           const string16& description,
           const ACMatchClassifications& description_class);
  // This constructor is used for creation of the structure from DB data.
  Shortcut(const std::string& id,
           const string16& text,
           const string16& url,
           const string16& contents,
           const string16& contents_class,
           const string16& description,
           const string16& description_class,
           int64 time_of_last_access,
           int   number_of_hits);
  // Required for STL, we don't use this directly.
  Shortcut();
  ~Shortcut();

  string16 contents_class_as_str() const;
  string16 description_class_as_str() const;

  std::string id;  // Unique guid for the shortcut.
  string16 text;  // The user's original input string.
  GURL url;       // The corresponding destination URL.

  // Contents and description from the original match, along with their
  // corresponding markup. We need these in order to correctly mark which
  // parts are URLs, dim, etc. However, we strip all MATCH classifications
  // from these since we'll mark the matching portions ourselves as we match
  // the user's current typing against these Shortcuts.
  string16 contents;
  ACMatchClassifications contents_class;
  string16 description;
  ACMatchClassifications description_class;

  base::Time last_access_time;  // Last time shortcut was selected.
  int number_of_hits;           // How many times shortcut was selected.
};

// Maps the original match (|text| in the Shortcut) to Shortcut for quick
// search.
typedef std::multimap<string16, Shortcut> ShortcutMap;

// Quick access guid maps - first one for loading, the second one is a shadow
// map for access.
typedef std::map<std::string, Shortcut> GuidToShortcutMap;
typedef std::map<std::string, ShortcutMap::iterator> GuidToShortcutsIteratorMap;

// Helpers dealing with database update.
// Converts spans vector to comma-separated string.
string16 SpansToString(const ACMatchClassifications& value);
// Coverts comma-separated unsigned integer values into spans vector.
ACMatchClassifications SpansFromString(const string16& value);

// Helper for initialization and update.
// Adds match at the end if and only if its style is different from the last
// match.
void AddLastMatchIfNeeded(ACMatchClassifications* matches,
                          int position,
                          int style);
}  // namespace shortcuts_provider

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_SHORTCUT_H_
