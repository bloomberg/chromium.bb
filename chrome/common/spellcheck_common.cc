// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/spellcheck_common.h"

#include "base/file_path.h"

namespace SpellCheckCommon {

static const struct {
  // The language.
  const char* language;

  // The corresponding language and region, used by the dictionaries.
  const char* language_region;
} g_supported_spellchecker_languages[] = {
  // Several languages are not to be included in the spellchecker list:
  // th-TH, uk-UA
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
  {"fr", "fr-FR"},
  {"he", "he-IL"},
  {"hi", "hi-IN"},
  {"hr", "hr-HR"},
  {"hu", "hu-HU"},
  {"id", "id-ID"},
  {"it", "it-IT"},
  {"lt", "lt-LT"},
  {"lv", "lv-LV"},
  {"nb", "nb-NO"},
  {"nl", "nl-NL"},
  {"pl", "pl-PL"},
  {"pt-BR", "pt-BR"},
  {"pt-PT", "pt-PT"},
  {"ro", "ro-RO"},
  {"ru", "ru-RU"},
  {"sk", "sk-SK"},
  {"sl", "sl-SI"},
  {"sh", "sh"},
  {"sr", "sr"},
  {"sv", "sv-SE"},
  {"tr", "tr-TR"},
  {"uk", "uk-UA"},
  {"vi", "vi-VN"},
};

// This function returns the language-region version of language name.
// e.g. returns hi-IN for hi.
std::string GetSpellCheckLanguageRegion(const std::string& input_language) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
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
  // The default dictionary version is 1-2. These versions have been augmented
  // with additional words found by the translation team.
  static const char kDefaultVersionString[] = "-1-2";

  static const struct {
    // The language input.
    const char* language;

    // The corresponding version.
    const char* version;
  } special_version_string[] = {
    {"es-ES", "-1-1"},  // 1-1: Have not been augmented with addtional words.
    {"nl-NL", "-1-1"},
    {"sv-SE", "-1-1"},
    {"he-IL", "-1-1"},
    {"el-GR", "-1-1"},
    {"hi-IN", "-1-1"},
    {"tr-TR", "-1-1"},
    {"et-EE", "-1-1"},
    {"lt-LT", "-1-3"},  // 1-3 (Feb 2009): new words, as well as an upgraded
                        // dictionary.
    {"pl-PL", "-1-3"},
    {"fr-FR", "-2-0"},  // 2-0 (2010): upgraded dictionaries.
    {"hu-HU", "-2-0"},
    {"ro-RO", "-2-0"},
    {"ru-RU", "-2-0"},
    {"bg-BG", "-2-0"},
    {"sr",    "-2-0"},
    {"uk-UA", "-2-0"},
    {"en-US", "-2-1"},  // 2-1 (Mar 2011): upgraded dictionaries.
    {"en-CA", "-2-1"},
    {"pt-BR", "-2-2"},  // 2-2 (Mar 2011): upgraded a dictionary.
    {"sh",    "-2-2"},  // 2-2 (Mar 2011): added a dictionary.
  };

  // Generate the bdict file name using default version string or special
  // version string, depending on the language.
  std::string language = GetSpellCheckLanguageRegion(input_language);
  std::string versioned_bdict_file_name(language + kDefaultVersionString +
                                        ".bdic");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(special_version_string); ++i) {
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
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
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

  // Look for a match by comparing only language parts. All the 'en-RR'
  // except for 'en-GB' exactly matched in the above loop, will match
  // 'en-US'. This is not ideal because 'en-ZA', 'en-NZ' had
  // better be matched with 'en-GB'. This does not handle cases like
  // 'az-Latn-AZ' vs 'az-Arab-AZ', either, but we don't use 3-part
  // locale ids with a script code in the middle, yet.
  // TODO(jungshik): Add a better fallback.
  std::string language_part(language, 0, language.find('-'));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    std::string spellcheck_language(
        g_supported_spellchecker_languages[i].language_region);
    if (spellcheck_language.substr(0, spellcheck_language.find('-')) ==
        language_part) {
      return spellcheck_language;
    }
  }

  // No match found - return blank.
  return std::string();
}


void SpellCheckLanguages(std::vector<std::string>* languages) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    languages->push_back(g_supported_spellchecker_languages[i].language);
  }
}

}  // namespace SpellCheckCommon
