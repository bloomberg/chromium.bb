// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/spellcheck_common.h"

#include "base/file_path.h"

namespace chrome {
namespace spellcheck_common {

struct LanguageRegion {
  const char* language;  // The language.
  const char* language_region;  // language & region, used by dictionaries.
};

struct LanguageVersion {
  const char* language;  // The language input.
  const char* version;   // The corresponding version.
};

static const LanguageRegion g_supported_spellchecker_languages[] = {
  // Several languages are not to be included in the spellchecker list:
  // th-TH, vi-VI.
  {"af", "af-ZA"},
  {"bg", "bg-BG"},
  {"ca", "ca-ES"},
  {"cs", "cs-CZ"},
  {"da", "da-DK"},
  {"de", "de-DE"},
  {"el", "el-GR"},
  {"en-AU", "en-AU"},
  {"en-CA", "en-CA"},
  {"en-GB", "en-GB"},
  {"en-US", "en-US"},
  {"es", "es-ES"},
  {"et", "et-EE"},
  {"fo", "fo-FO"},
  {"fr", "fr-FR"},
  {"he", "he-IL"},
  {"hi", "hi-IN"},
  {"hr", "hr-HR"},
  {"hu", "hu-HU"},
  {"id", "id-ID"},
  {"it", "it-IT"},
  {"ko", "ko"},
  {"lt", "lt-LT"},
  {"lv", "lv-LV"},
  {"nb", "nb-NO"},
  {"nl", "nl-NL"},
  {"pl", "pl-PL"},
  {"pt-BR", "pt-BR"},
  {"pt-PT", "pt-PT"},
  {"ro", "ro-RO"},
  {"ru", "ru-RU"},
  {"sh", "sh"},
  {"sk", "sk-SK"},
  {"sl", "sl-SI"},
  {"sq", "sq"},
  {"sr", "sr"},
  {"sv", "sv-SE"},
  {"ta", "ta-IN"},
  {"tr", "tr-TR"},
  {"uk", "uk-UA"},
  {"vi", "vi-VN"},
};

bool IsValidRegion(const std::string& region) {
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    if (g_supported_spellchecker_languages[i].language_region == region)
      return true;
  }
  return false;
}

// This function returns the language-region version of language name.
// e.g. returns hi-IN for hi.
std::string GetSpellCheckLanguageRegion(const std::string& input_language) {
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    if (g_supported_spellchecker_languages[i].language == input_language) {
      return std::string(
          g_supported_spellchecker_languages[i].language_region);
    }
  }

  return input_language;
}

FilePath GetVersionedFileName(const std::string& input_language,
                              const FilePath& dict_dir) {
  // The default dictionary version is 3-0. This version indicates that the bdic
  // file contains a checksum.
  static const char kDefaultVersionString[] = "-3-0";

  // Add non-default version strings here. Use the same version for all the
  // dictionaries that you add at the same time. Increment the major version
  // number if you're updating either dic or aff files. Increment the minor
  // version number if you're updating only dic_delta files.
  std::vector<LanguageVersion> special_version_string;

  // Generate the bdict file name using default version string or special
  // version string, depending on the language.
  std::string language = GetSpellCheckLanguageRegion(input_language);
  std::string versioned_bdict_file_name(language + kDefaultVersionString +
                                        ".bdic");
  for (size_t i = 0; i < special_version_string.size(); ++i) {
    if (language == special_version_string[i].language) {
      versioned_bdict_file_name =
          language + special_version_string[i].version + ".bdic";
      break;
    }
  }

  return dict_dir.AppendASCII(versioned_bdict_file_name);
}

std::string GetCorrespondingSpellCheckLanguage(const std::string& language) {
  // Look for exact match in the Spell Check language list.
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    // First look for exact match in the language region of the list.
    std::string spellcheck_language(
        g_supported_spellchecker_languages[i].language);
    if (spellcheck_language == language)
      return language;

    // Next, look for exact match in the language_region part of the list.
    std::string spellcheck_language_region(
        g_supported_spellchecker_languages[i].language_region);
    if (spellcheck_language_region == language)
      return g_supported_spellchecker_languages[i].language;
  }

  // No match found - return blank.
  return std::string();
}

void SpellCheckLanguages(std::vector<std::string>* languages) {
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    languages->push_back(g_supported_spellchecker_languages[i].language);
  }
}

}  // namespace spellcheck_common
}  // namespace chrome
