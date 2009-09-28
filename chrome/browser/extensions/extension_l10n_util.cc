// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_l10n_util.h"

#include <set>
#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/json_value_serializer.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extension_l10n_util {

std::string GetDefaultLocaleFromManifest(const DictionaryValue& manifest,
                                         std::string* error) {
  std::string default_locale;
  if (!manifest.GetString(keys::kDefaultLocale, &default_locale)) {
    *error = errors::kInvalidDefaultLocale;
    return "";
  }
  // Normalize underscores to hyphens.
  std::replace(default_locale.begin(), default_locale.end(), '_', '-');
  return default_locale;
}

bool AddLocale(const std::set<std::string>& chrome_locales,
               const FilePath& locale_folder,
               std::set<std::string>* valid_locales,
               std::string* locale_name,
               std::string* error) {
  // Normalize underscores to hyphens because that's what our locale files use.
  std::replace(locale_name->begin(), locale_name->end(), '_', '-');
  // Accept name that starts with a . but don't add it to the list of supported
  // locales.
  if (locale_name->find(".") == 0)
    return true;
  if (chrome_locales.find(*locale_name) == chrome_locales.end()) {
    // Fail if there is an extension locale that's not in the Chrome list.
    *error = StringPrintf("Supplied locale %s is not supported.",
                          locale_name->c_str());
    return false;
  }
  // Check if messages file is actually present (but don't check content).
  if (file_util::PathExists(
         locale_folder.AppendASCII(Extension::kMessagesFilename))) {
    valid_locales->insert(*locale_name);
  } else {
    *error = StringPrintf("Catalog file is missing for locale %s.",
                          locale_name->c_str());
    return false;
  }

  return true;
}

bool GetValidLocales(const FilePath& locale_path,
                     std::set<std::string>* valid_locales,
                     std::string* error) {
  // Get available chrome locales as a set.
  const std::vector<std::string>& available_locales =
      l10n_util::GetAvailableLocales();
  static std::set<std::string> chrome_locales(available_locales.begin(),
                                              available_locales.end());
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
                   valid_locales,
                   &locale_name,
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
  // Normalize hyphens to underscores because that's what our locale files use.
  std::replace(extension_locale.begin(), extension_locale.end(), '-', '_');
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
    std::string* error) {
  scoped_ptr<DictionaryValue> default_catalog(
      LoadMessageFile(locale_path, default_locale, error));
  if (!default_catalog.get()) {
    return false;
  }

  scoped_ptr<DictionaryValue> app_catalog(
      LoadMessageFile(locale_path, application_locale, error));
  if (!app_catalog.get()) {
    // Only default catalog has to be present. This is not an error.
    app_catalog.reset(new DictionaryValue);
    error->clear();
  }

  return ExtensionMessageBundle::Create(*default_catalog,
                                        *app_catalog,
                                        error);
}

}  // namespace extension_l10n_util
