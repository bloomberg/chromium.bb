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
  virtual ~LanguageOptionsHandlerCommon();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Uninitialize() OVERRIDE;

  // DOMMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // SpellcheckHunspellDictionary::Observer implementation.
  virtual void OnHunspellDictionaryInitialized() OVERRIDE;
  virtual void OnHunspellDictionaryDownloadBegin() OVERRIDE;
  virtual void OnHunspellDictionaryDownloadSuccess() OVERRIDE;
  virtual void OnHunspellDictionaryDownloadFailure() OVERRIDE;

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
  // Returns the name of the product (ex. "Chrome" or "Chrome OS").
  virtual base::string16 GetProductName() = 0;

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
  // has failed to download.
  void RetrySpellcheckDictionaryDownload(const base::ListValue* args);

  // Called when the user saves the language list preferences.
  void UpdateLanguageListCallback(const base::ListValue* args);

  // Updates the hunspell dictionary that is used for spellchecking.
  void RefreshHunspellDictionary();

  // Returns the hunspell dictionary that is used for spellchecking. Never null.
  base::WeakPtr<SpellcheckHunspellDictionary>& GetHunspellDictionary();

  // The hunspell dictionary that is used for spellchecking. Might be null.
  base::WeakPtr<SpellcheckHunspellDictionary> hunspell_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandlerCommon);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_COMMON_H_
