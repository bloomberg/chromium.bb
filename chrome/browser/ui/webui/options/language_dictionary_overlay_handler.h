// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_DICTIONARY_OVERLAY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_DICTIONARY_OVERLAY_HANDLER_H_

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

class LanguageDictionaryOverlayHandler
    : public OptionsPageUIHandler,
      public SpellcheckCustomDictionary::Observer {
 public:
  LanguageDictionaryOverlayHandler();
  virtual ~LanguageDictionaryOverlayHandler();

  // Overridden from OptionsPageUIHandler:
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;
  virtual void Uninitialize() OVERRIDE;

  // Overridden from SpellcheckCustomDictionary::Observer:
  virtual void OnCustomDictionaryLoaded() OVERRIDE;
  virtual void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& dictionary_change) OVERRIDE;

 private:
  // Sends the dictionary words to WebUI.
  void ResetDictionaryWords();

  // Refreshes the displayed words. Called from WebUI.
  void RefreshWords(const base::ListValue* args);

  // Adds a new word to the dictionary. Called from WebUI.
  void AddWord(const base::ListValue* args);

  // Removes a word from the dictionary. Called from WebUI.
  void RemoveWord(const base::ListValue* args);

  // Whether the overlay is initialized and ready to show data.
  bool overlay_initialized_;

  // The custom spelling dictionary. Used for adding, removing, and reading
  // words in custom dictionary file.
  SpellcheckCustomDictionary* dictionary_;

  DISALLOW_COPY_AND_ASSIGN(LanguageDictionaryOverlayHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_DICTIONARY_OVERLAY_HANDLER_H_
