// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_l10n_util.h"

#include <set>
#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/linked_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/json_value_serializer.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

static std::string* GetProcessLocale() {
  static std::string locale;
  return &locale;
}

namespace extension_l10n_util {

void SetProcessLocale(const std::string& locale) {
  *(GetProcessLocale()) = locale;
}

std::string GetDefaultLocaleFromManifest(const DictionaryValue& manifest,
                                         std::string* error) {
  std::string default_locale;
  if (!manifest.GetString(keys::kDefaultLocale, &default_locale)) {
    *error = errors::kInvalidDefaultLocale;
    return "";
  }

  return default_locale;
}

bool AddLocale(const std::set<std::string>& chrome_locales,
               const FilePath& locale_folder,
               const std::string& locale_name,
               std::set<std::string>* valid_locales,
               std::string* error) {
  // Accept name that starts with a . but don't add it to the list of supported
  // locales.
  if (locale_name.find(".") == 0)
    return true;
  if (chrome_locales.find(locale_name) == chrome_locales.end()) {
    // Fail if there is an extension locale that's not in the Chrome list.
    *error = StringPrintf("Supplied locale %s is not supported.",
                          locale_name.c_str());
    return false;
  }
  // Check if messages file is actually present (but don't check content).
  if (file_util::PathExists(
         locale_folder.AppendASCII(Extension::kMessagesFilename))) {
    valid_locales->insert(locale_name);
  } else {
    *error = StringPrintf("Catalog file is missing for locale %s.",
                          locale_name.c_str());
    return false;
  }

  return true;
}

std::string NormalizeLocale(const std::string& locale) {
  std::string normalized_locale(locale);
  std::replace(normalized_locale.begin(), normalized_locale.end(), '-', '_');

  return normalized_locale;
}

void GetParentLocales(const std::string& current_locale,
                      std::vector<std::string>* parent_locales) {
  std::string locale(NormalizeLocale(current_locale));

  const int kNameCapacity = 256;
  char parent[kNameCapacity];
  base::strlcpy(parent, locale.c_str(), kNameCapacity);
  parent_locales->push_back(parent);
  UErrorCode err = U_ZERO_ERROR;
  while (uloc_getParent(parent, parent, kNameCapacity, &err) > 0) {
    if (err != U_ZERO_ERROR)
      break;
    parent_locales->push_back(parent);
  }
}

// Extends list of Chrome locales to them and their parents, so we can do
// proper fallback.
static void GetAllLocales(std::set<std::string>* all_locales) {
  const std::vector<std::string>& available_locales =
      l10n_util::GetAvailableLocales();
  // Add all parents of the current locale to the available locales set.
  // I.e. for sr_Cyrl_RS we add sr_Cyrl_RS, sr_Cyrl and sr.
  for (size_t i = 0; i < available_locales.size(); ++i) {
    std::vector<std::string> result;
    GetParentLocales(available_locales[i], &result);
    all_locales->insert(result.begin(), result.end());
  }
}

bool GetValidLocales(const FilePath& locale_path,
                     std::set<std::string>* valid_locales,
                     std::string* error) {
  static std::set<std::string> chrome_locales;
  GetAllLocales(&chrome_locales);

  // Enumerate all supplied locales in the extension.
  file_util::FileEnumerator locales(locale_path,
                                    false,
                                    file_util::FileEnumerator::DIRECTORIES);
  FilePath locale_folder;
  while (!(locale_folder = locales.Next()).empty()) {
    std::string locale_name =
        WideToASCII(locale_folder.BaseName().ToWStringHack());
    if (!AddLocale(chrome_locales,
                   locale_folder,
                   locale_name,
                   valid_locales,
                   error)) {
      return false;
    }
  }

  if (valid_locales->empty()) {
    *error = extension_manifest_errors::kLocalesNoValidLocaleNamesListed;
    return false;
  }

  return true;
}

// Loads contents of the messages file for given locale. If file is not found,
// or there was parsing error we return NULL and set |error|.
// Caller owns the returned object.
static DictionaryValue* LoadMessageFile(const FilePath& locale_path,
                                        const std::string& locale,
                                        std::string* error) {

  std::string extension_locale = locale;
  FilePath file = locale_path.AppendASCII(extension_locale)
      .AppendASCII(Extension::kMessagesFilename);
  JSONFileValueSerializer messages_serializer(file);
  Value *dictionary = messages_serializer.Deserialize(error);
  if (!dictionary && error->empty()) {
    // JSONFileValueSerializer just returns NULL if file cannot be found. It
    // doesn't set the error, so we have to do it.
    *error = StringPrintf("Catalog file is missing for locale %s.",
                          extension_locale.c_str());
  }

  return static_cast<DictionaryValue*>(dictionary);
}

ExtensionMessageBundle* LoadMessageCatalogs(
    const FilePath& locale_path,
    const std::string& default_locale,
    const std::string& application_locale,
    const std::set<std::string>& valid_locales,
    std::string* error) {
  // Order locales to load as current_locale, first_parent, ..., default_locale.
  std::vector<std::string> all_fallback_locales;
  if (!application_locale.empty() && application_locale != default_locale)
    GetParentLocales(application_locale, &all_fallback_locales);
  all_fallback_locales.push_back(default_locale);

  std::vector<linked_ptr<DictionaryValue> > catalogs;
  for (size_t i = 0; i < all_fallback_locales.size(); ++i) {
    // Skip all parent locales that are not supplied.
    if (valid_locales.find(all_fallback_locales[i]) == valid_locales.end())
      continue;
    linked_ptr<DictionaryValue> catalog(
      LoadMessageFile(locale_path, all_fallback_locales[i], error));
    if (!catalog.get()) {
      // If locale is valid, but messages.json is corrupted or missing, return
      // an error.
      return false;
    } else {
      catalogs.push_back(catalog);
    }
  }

  return ExtensionMessageBundle::Create(catalogs, error);
}

void GetL10nRelativePaths(const FilePath& relative_resource_path,
                          std::vector<FilePath>* l10n_paths) {
  DCHECK(NULL != l10n_paths);

  std::string* current_locale = GetProcessLocale();
  if (current_locale->empty())
    *current_locale = l10n_util::GetApplicationLocale(L"");

  std::vector<std::string> locales;
  GetParentLocales(*current_locale, &locales);

  FilePath locale_relative_path;
  for (size_t i = 0; i < locales.size(); ++i) {
    l10n_paths->push_back(locale_relative_path
        .AppendASCII(Extension::kLocaleFolder)
        .AppendASCII(locales[i])
        .Append(relative_resource_path));
  }
}

}  // namespace extension_l10n_util
