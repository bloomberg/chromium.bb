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

namespace keys = extension_manifest_keys;

namespace extension_l10n_util {

bool ValidateDefaultLocale(const Extension* extension) {
  std::string default_locale = extension->default_locale();

  if (extension->supported_locales().find(default_locale) !=
        extension->supported_locales().end()) {
    return true;
  } else {
    return false;
  }
}


bool AddLocale(const std::set<std::string>& chrome_locales,
               const FilePath& locale_folder,
               Extension* extension,
               std::string* locale_name,
               std::string* error) {
  // Normalize underscores to hyphens because that's what our locale files use.
  std::replace(locale_name->begin(), locale_name->end(), '_', '-');
  if (chrome_locales.find(*locale_name) == chrome_locales.end()) {
    // Fail if there is an extension locale that's not in the Chrome list.
    *error = StringPrintf("Supplied locale %s is not supported.",
                          locale_name->c_str());
    return false;
  }
  // Check if messages file is actually present (but don't check content).
  if (file_util::PathExists(
         locale_folder.AppendASCII(Extension::kMessagesFilename))) {
    extension->AddSupportedLocale(*locale_name);
  } else {
    *error = StringPrintf("Catalog file is missing for locale %s.",
                          locale_name->c_str());
    return false;
  }

  return true;
}

bool AddValidLocales(const FilePath& locale_path,
                     Extension* extension,
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
    if (!AddLocale(chrome_locales, locale_folder,
                   extension, &locale_name, error)) {
      return false;
    }
  }

  if (extension->supported_locales().empty()) {
    *error = extension_manifest_errors::kLocalesNoValidLocaleNamesListed;
    return false;
  }

  return true;
}

}  // namespace extension_l10n_util
