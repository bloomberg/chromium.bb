// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/dom_ui/options_ui.h"

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

 private:
  // Gets the list of input methods from the given input descriptors.
  // The return value will look like:
  // [{'id': 'pinyin', 'displayName': 'Pinyin', 'languageCode': 'zh-CW'}, ...]
  ListValue* GetInputMethodList(const InputMethodDescriptors& descriptors);

  // Gets the list of languages from the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  ListValue* GetLanguageList(const InputMethodDescriptors& descriptors);

  // Called when the UI language is changed.
  // |value| will be the language code as string (ex. "fr").
  void UiLanguageChangeCallback(const Value* value);

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandler);
};

}  // namespace

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
