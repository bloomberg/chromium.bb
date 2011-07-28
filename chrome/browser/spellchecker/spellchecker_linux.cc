// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If linux ever gains a platform specific spellchecker, it will be
// implemented here.

#include "spellchecker_platform_engine.h"

namespace SpellCheckerPlatform {

bool SpellCheckerAvailable() {
  // As of Summer 2009, there is no commonly accepted platform spellchecker
  // for Linux, so we'll return false here.
  return false;
}

// The following methods are just stubs to keep the linker happy.
bool PlatformSupportsLanguage(const std::string& current_language) {
  return false;
}

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
  spellcheck_languages->clear();
}

bool SpellCheckerProvidesPanel() {
  return false;
}

bool SpellingPanelVisible() {
  return false;
}

void ShowSpellingPanel(bool show) {}

void UpdateSpellingPanelWithMisspelledWord(const string16& word) {}

void Init() {}

void SetLanguage(const std::string& lang_to_set) {}

bool CheckSpelling(const string16& word_to_check, int tag) {
  return false;
}

void FillSuggestionList(const string16& wrong_word,
                        std::vector<string16>* optional_suggestions) {}

void AddWord(const string16& word) {}

void RemoveWord(const string16& word) {}

int GetDocumentTag() { return 0; }

void IgnoreWord(const string16& word) {}

void CloseDocumentWithTag(int tag) {}

void RequestTextCheck(int route_id,
                      int identifier,
                      int document_tag,
                      const string16& text,
                      BrowserMessageFilter* destination) {}

}  // namespace SpellCheckerPlatform
