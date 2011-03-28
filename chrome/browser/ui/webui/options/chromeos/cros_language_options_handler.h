// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/ui/webui/options/language_options_handler.h"

namespace chromeos {

// Language options page UI handler for Chrome OS.  For non-Chrome OS,
// see LanguageOptionsHnadler.
class CrosLanguageOptionsHandler : public LanguageOptionsHandlerCommon {
 public:
  CrosLanguageOptionsHandler();
  virtual ~CrosLanguageOptionsHandler();

  // OptionsPageUIHandler implementation.
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
      const chromeos::InputMethodDescriptors& descriptors);

  // Gets the list of languages from the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  static ListValue* GetLanguageList(
      const chromeos::InputMethodDescriptors& descriptors);

 private:
  // LanguageOptionsHandlerCommon implementation.
  virtual string16 GetProductName();
  virtual void SetApplicationLocale(const std::string& language_code);

  // Called when the sign-out button is clicked.
  void RestartCallback(const ListValue* args);

  // Called when the input method is disabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodDisableCallback(const ListValue* args);

  // Called when the input method is enabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodEnableCallback(const ListValue* args);

  // Called when the input method options page is opened.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodOptionsOpenCallback(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(CrosLanguageOptionsHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
