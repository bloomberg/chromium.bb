// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_COMMON_H_
#define CHROME_COMMON_SPELLCHECK_COMMON_H_
#pragma once

#include <string>
#include <vector>

class FilePath;

namespace SpellCheckCommon {

// Max number of dictionary suggestions.
static const int kMaxSuggestions = 5;

static const int kMaxAutoCorrectWordSize = 8;

FilePath GetVersionedFileName(const std::string& input_language,
                              const FilePath& dict_dir);

std::string GetCorrespondingSpellCheckLanguage(const std::string& language);

// Get SpellChecker supported languages.
void SpellCheckLanguages(std::vector<std::string>* languages);


// This function returns ll (language code) from ll-RR where 'RR' (region
// code) is redundant. However, if the region code matters, it's preserved.
// That is, it returns 'hi' and 'en-GB' for 'hi-IN' and 'en-GB' respectively.
std::string GetLanguageFromLanguageRegion(std::string input_language);

}  // namespace SpellCheckCommon

#endif  // CHROME_COMMON_SPELLCHECK_COMMON_H_
