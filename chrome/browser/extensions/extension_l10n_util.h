// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares extension specific l10n utils.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_L10N_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_L10N_UTIL_H_

#include <set>
#include <string>

class DictionaryValue;
class Extension;
class FilePath;

namespace extension_l10n_util {

// Returns true if default_locale was set to valid locale
// (supported by the extension).
bool ValidateDefaultLocale(const Extension* extension);

// Adds locale_name to the extension if it's in chrome_locales, and
// if messages file is present (we don't check content of messages file here).
// Returns false if locale_name was not found in chrome_locales, and sets
// error with locale_name.
bool AddLocale(const std::set<std::string>& chrome_locales,
               const FilePath& locale_folder,
               Extension* extension,
               std::string* locale_name,
               std::string* error);

// Adds valid locales to the extension.
// 1. Do nothing if _locales directory is missing (not an error).
// 2. Get list of Chrome locales.
// 3. Enumerate all subdirectories of _locales directory.
// 4. Intersect both lists, and add intersection to the extension.
// Returns false if any of supplied locales don't match chrome list of locales.
// Fills out error with offending locale name.
bool AddValidLocales(const FilePath& locale_path,
                     Extension* extension,
                     std::string* error);

}  // namespace extension_l10n_util

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_L10N_UTIL_H_
