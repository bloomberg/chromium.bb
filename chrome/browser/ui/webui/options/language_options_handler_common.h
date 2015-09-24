// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_COMMON_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_COMMON_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace options {

// The base class for language options page UI handlers.  This class has code
// common to the Chrome OS and non-Chrome OS implementation of the handler.
class LanguageOptionsHandlerCommon
    : public OptionsPageUIHandler,
      public SpellcheckHunspellDictionary::Observer {
 public:
  LanguageOptionsHandlerCommon();
  ~LanguageOptionsHandlerCommon() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void Uninitialize() override;

  // DOMMessageHandler implementation.
  void RegisterMessages() override;

  // SpellcheckHunspellDictionary::Observer implementation.
  void OnHunspellDictionaryInitialized(const std::string& language) override;
  void OnHunspellDictionaryDownloadBegin(const std::string& language) override;
  void OnHunspellDictionaryDownloadSuccess(
      const std::string& language) override;
  void OnHunspellDictionaryDownloadFailure(
      const std::string& language) override;

  // The following static methods are public for ease of testing.

  // Gets the set of language codes that can be used as UI language.
  // The return value will look like:
  // {'en-US': true, 'fi': true, 'fr': true, ...}
  //
  // Note that true in values does not mean anything. We just use the
  // dictionary as a set.
  static base::DictionaryValue* GetUILanguageCodeSet();

  // Gets the set of language codes that can be used for spellchecking.
  // The return value will look like:
  // {'en-US': true, 'fi': true, 'fr': true, ...}
  //
  // Note that true in values does not mean anything. We just use the
  // dictionary as a set.
  static base::DictionaryValue* GetSpellCheckLanguageCodeSet();

 private:
  // Sets the application locale.
  virtual void SetApplicationLocale(const std::string& language_code) = 0;

  // Called when the language options is opened.
  void LanguageOptionsOpenCallback(const base::ListValue* args);

  // Called when the UI language is changed.
  // |args| will contain the language code as string (ex. "fr").
  void UiLanguageChangeCallback(const base::ListValue* args);

  // Called when the spell check language is changed.
  // |args| will contain the language code as string (ex. "fr").
  void SpellCheckLanguageChangeCallback(const base::ListValue* args);

  // Called when the user clicks "Retry" button for a spellcheck dictionary that
  // has failed to download. |args| will contain the language code as a string
  // (ex. "fr").
  void RetrySpellcheckDictionaryDownload(const base::ListValue* args);

  // Called when the user saves the language list preferences.
  void UpdateLanguageListCallback(const base::ListValue* args);

  // Returns a pointer to the browser SpellcheckService. Can be null if the
  // service has already shut down.
  SpellcheckService* GetSpellcheckService();

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandlerCommon);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_COMMON_H_
