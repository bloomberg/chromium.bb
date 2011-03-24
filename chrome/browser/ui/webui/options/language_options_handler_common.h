// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_COMMON_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_COMMON_H_
#pragma once

#include "chrome/browser/ui/webui/options/options_ui.h"

class DictionaryValue;
class ListValue;

// The base class for language options page UI handlers.  This class has code
// common to the Chrome OS and non-Chrome OS implementation of the handler.
class LanguageOptionsHandlerCommon : public OptionsPageUIHandler {
 public:
  LanguageOptionsHandlerCommon();
  virtual ~LanguageOptionsHandlerCommon();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // The following static methods are public for ease of testing.

  // Gets the set of language codes that can be used as UI language.
  // The return value will look like:
  // {'en-US': true, 'fi': true, 'fr': true, ...}
  //
  // Note that true in values does not mean anything. We just use the
  // dictionary as a set.
  static DictionaryValue* GetUILanguageCodeSet();

  // Gets the set of language codes that can be used for spellchecking.
  // The return value will look like:
  // {'en-US': true, 'fi': true, 'fr': true, ...}
  //
  // Note that true in values does not mean anything. We just use the
  // dictionary as a set.
  static DictionaryValue* GetSpellCheckLanguageCodeSet();

 private:
  // Returns the name of the product (ex. "Chrome" or "Chrome OS").
  virtual string16 GetProductName() = 0;

  // Sets the application locale.
  virtual void SetApplicationLocale(const std::string& language_code) = 0;

  // Called when the language options is opened.
  void LanguageOptionsOpenCallback(const ListValue* args);

  // Called when the UI language is changed.
  // |args| will contain the language code as string (ex. "fr").
  void UiLanguageChangeCallback(const ListValue* args);

  // Called when the spell check language is changed.
  // |args| will contain the language code as string (ex. "fr").
  void SpellCheckLanguageChangeCallback(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandlerCommon);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_COMMON_H_
