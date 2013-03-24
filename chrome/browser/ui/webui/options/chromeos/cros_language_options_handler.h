// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/options/language_options_handler.h"
#include "chromeos/ime/input_method_descriptor.h"

namespace chromeos {
namespace options {

// Language options page UI handler for Chrome OS.  For non-Chrome OS,
// see LanguageOptionsHnadler.
class CrosLanguageOptionsHandler
    : public ::options::LanguageOptionsHandlerCommon {
 public:
  CrosLanguageOptionsHandler();
  virtual ~CrosLanguageOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // DOMMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // The following static methods are public for ease of testing.

  // Gets the list of input methods from the given input descriptors.
  // The return value will look like:
  // [{'id': 'pinyin', 'displayName': 'Pinyin',
  //   'languageCodeSet': {'zh-CW': true}},  ...]
  //
  // Note that true in languageCodeSet does not mean anything. We just use
  // the dictionary as a set.
  static base::ListValue* GetInputMethodList(
      const input_method::InputMethodDescriptors& descriptors);

  // Gets the list of languages from the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  static base::ListValue* GetLanguageList(
      const input_method::InputMethodDescriptors& descriptors);

  // Gets the list of input methods that are implemented in an extension.
  // The return value will look like:
  // [{'id': '_ext_ime_nejguenhnsnjnwychcnsdsdjketest',
  //   'displayName': 'Sample IME'},  ...]
  static base::ListValue* GetExtensionImeList();

 private:
  // LanguageOptionsHandlerCommon implementation.
  virtual string16 GetProductName() OVERRIDE;
  virtual void SetApplicationLocale(const std::string& language_code) OVERRIDE;

  // Called when the sign-out button is clicked.
  void RestartCallback(const base::ListValue* args);

  // Called when the input method is disabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodDisableCallback(const base::ListValue* args);

  // Called when the input method is enabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodEnableCallback(const base::ListValue* args);

  // Called when the input method options page is opened.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodOptionsOpenCallback(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(CrosLanguageOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
