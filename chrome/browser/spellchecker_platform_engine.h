// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface that any platform-specific spellchecker
// needs to implement in order to be used by the browser.

#ifndef CHROME_BROWSER_SPELLCHECKER_PLATFORM_ENGINE_H_
#define CHROME_BROWSER_SPELLCHECKER_PLATFORM_ENGINE_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/string16.h"

class BrowserMessageFilter;

namespace SpellCheckerPlatform {

// Get the languages supported by the platform spellchecker and store them in
// |spellcheck_languages|. Note that they must be converted to
// Chromium style codes (en-US not en_US). See spellchecker.cc for a full list.
void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages);

// Returns true if there is a platform-specific spellchecker that can be used.
bool SpellCheckerAvailable();

// Returns true if the platform spellchecker has a spelling panel.
bool SpellCheckerProvidesPanel();

// Returns true if the platform spellchecker panel is visible.
bool SpellingPanelVisible();

// Shows the spelling panel if |show| is true and hides it if it is not.
void ShowSpellingPanel(bool show);

// Changes the word show in the spelling panel to be |word|. Note that the
// spelling panel need not be displayed for this to work.
void UpdateSpellingPanelWithMisspelledWord(const string16& word);

// Do any initialization needed for spellchecker.
void Init();
// TODO(pwicks): should we add a companion to this, TearDown or something?

// Translates the codes used by chrome to the language codes used by os x
// and checks the given language agains the languages that the current system
// supports. If the platform-specific spellchecker supports the language,
// then returns true, otherwise false.
bool PlatformSupportsLanguage(const std::string& current_language);

// Sets the language for the platform-specific spellchecker.
void SetLanguage(const std::string& lang_to_set);

// Checks the spelling of the given string, using the platform-specific
// spellchecker. Returns true if the word is spelled correctly.
bool CheckSpelling(const string16& word_to_check, int tag);

// Fills the given vector |optional_suggestions| with a number (up to
// kMaxSuggestions, which is defined in spellchecker_common.h) of suggestions
// for the string |wrong_word|.
void FillSuggestionList(const string16& wrong_word,
                        std::vector<string16>* optional_suggestions);

// Adds the given word to the platform dictionary.
void AddWord(const string16& word);

// Remove a given word from the platform dictionary.
void RemoveWord(const string16& word);

// Gets a unique tag to identify a document. Used in ignoring words.
int GetDocumentTag();

// Tells the platform spellchecker to ignore a word. This doesn't take a tag
// because in most of the situations in which it is called, the only way to know
// the tag for sure is to ask the renderer, which would mean blocking in the
// browser, so (on the mac, anyway) we remember the most recent tag and use
// it, since it should always be from the same document.
void IgnoreWord(const string16& word);

// Tells the platform spellchecker that a document associated with a tag has
// closed. Generally, this means that any ignored words associated with that
// document can now be forgotten.
void CloseDocumentWithTag(int tag);

// Requests an asyncronous spell and grammar checking.
// The result is returned to an IPC message to |destination| thus it should
// not be null.
void RequestTextCheck(int route_id,
                      int identifier,
                      int document_tag,
                      const string16& text,
                      BrowserMessageFilter* destination);

}  // namespace SpellCheckerPlatform

#endif  // CHROME_BROWSER_SPELLCHECKER_PLATFORM_ENGINE_H_
