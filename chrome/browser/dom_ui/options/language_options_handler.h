// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_
#pragma once

// TODO(csilv): This is for the move CL.  Changes to make this cross-platform
// will come in the followup CL.
#if defined(OS_CHROMEOS)

#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/dom_ui/options/options_ui.h"

class DictionaryValue;
class ListValue;

namespace chromeos {

// ChromeOS language options page UI handler.
class LanguageOptionsHandler : public OptionsPageUIHandler {
 public:
  LanguageOptionsHandler();
  virtual ~LanguageOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // The following static methods are public for ease of testing.

  // Gets the list of input methods from the given input descriptors.
  // The return value will look like:
  // [{'id': 'pinyin', 'displayName': 'Pinyin',
  //   'languageCodeSet': {'zh-CW': true}},  ...]
  //
  // Note that true in languageCodeSet does not mean anything. We just use
  // the dictionary as a set.
  static ListValue* GetInputMethodList(
      const InputMethodDescriptors& descriptors);

  // Gets the list of languages from the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  static ListValue* GetLanguageList(const InputMethodDescriptors& descriptors);

  // Gets the set of language codes that can be used as UI language.
  // The return value will look like:
  // {'en-US': true, 'fi': true, 'fr': true, ...}
  //
  // Note that true in values does not mean anything. We just use the
  // dictionary as a set.
  static DictionaryValue* GetUiLanguageCodeSet();

  // Gets the set of language codes that can be used for spellchecking.
  // The return value will look like:
  // {'en-US': true, 'fi': true, 'fr': true, ...}
  //
  // Note that true in values does not mean anything. We just use the
  // dictionary as a set.
  static DictionaryValue* GetSpellCheckLanguageCodeSet();

 private:
  // Called when the input method is disabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodDisableCallback(const ListValue* args);

  // Called when the input method is enabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodEnableCallback(const ListValue* args);

  // Called when the input method options page is opened.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodOptionsOpenCallback(const ListValue* args);

  // Called when the language options is opened.
  void LanguageOptionsOpenCallback(const ListValue* args);

  // Called when the UI language is changed.
  // |args| will contain the language code as string (ex. "fr").
  void UiLanguageChangeCallback(const ListValue* args);

  // Called when the spell check language is changed.
  // |args| will contain the language code as string (ex. "fr").
  void SpellCheckLanguageChangeCallback(const ListValue* args);

  // Called when the sign out button is clicked.
  void SignOutCallback(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandler);
};

}  // namespace

#endif  // OS_CHROMEOS

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_

