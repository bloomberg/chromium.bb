// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares extension specific l10n utils.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_L10N_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_L10N_UTIL_H_

#include <set>
#include <string>
#include <vector>

class DictionaryValue;
class Extension;
class ExtensionMessageBundle;
class FilePath;

namespace extension_l10n_util {

// Returns default locale in form "en-US" or "sr" or empty string if
// "default_locale" section was not defined in the manifest.json file.
std::string GetDefaultLocaleFromManifest(const DictionaryValue& manifest,
                                         std::string* error);

// Adds locale_name to the extension if it's in chrome_locales, and
// if messages file is present (we don't check content of messages file here).
// Returns false if locale_name was not found in chrome_locales, and sets
// error with locale_name.
// If file name starts with . return true (helps testing extensions under svn).
bool AddLocale(const std::set<std::string>& chrome_locales,
               const FilePath& locale_folder,
               const std::string& locale_name,
               std::set<std::string>* valid_locales,
               std::string* error);

// Converts all - into _, to be consistent with ICU and file system names.
std::string NormalizeLocale(const std::string& locale);

// Produce a vector of parent locales for given locale.
// It includes the current locale in the result.
// sr_Cyrl_RS generates sr_Cyrl_RS, sr_Cyrl and sr.
void GetParentLocales(const std::string& current_locale,
                      std::vector<std::string>* parent_locales);

// Adds valid locales to the extension.
// 1. Do nothing if _locales directory is missing (not an error).
// 2. Get list of Chrome locales.
// 3. Enumerate all subdirectories of _locales directory.
// 4. Intersect both lists, and add intersection to the extension.
// Returns false if any of supplied locales don't match chrome list of locales.
// Fills out error with offending locale name.
bool GetValidLocales(const FilePath& locale_path,
                     std::set<std::string>* locales,
                     std::string* error);

// Loads messages file for default locale, and application locales (application
// locales doesn't have to exist). Application locale is current locale and its
// parents.
// Returns message bundle if it can load default locale messages file, and all
// messages are valid, else returns NULL and sets error.
ExtensionMessageBundle* LoadMessageCatalogs(
    const FilePath& locale_path,
    const std::string& default_locale,
    const std::string& app_locale,
    const std::set<std::string>& valid_locales,
    std::string* error);

// Returns relative l10n paths to the resource.
// Returned vector starts with more specific locale path, and ends with the
// least specific one.
void GetL10nRelativePaths(const FilePath& relative_resource_path,
                          std::vector<FilePath>* l10n_paths);

}  // namespace extension_l10n_util

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_L10N_UTIL_H_
