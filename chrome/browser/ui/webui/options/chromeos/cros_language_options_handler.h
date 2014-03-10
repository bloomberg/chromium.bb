// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/options/language_options_handler.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/input_method_descriptor.h"

namespace chromeos {
namespace options {

// GetUILanguageList() returns concatenated list of list of vendor languages
// followed by other languages. An entry with "code" attribute set to this value
// is inserted in between.
extern const char kVendorOtherLanguagesListDivider[];

// Language options page UI handler for Chrome OS.  For non-Chrome OS,
// see LanguageOptionsHnadler.
class CrosLanguageOptionsHandler
    : public ::options::LanguageOptionsHandlerCommon,
      public ComponentExtensionIMEManager::Observer {
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

  // Gets the list of accept languages with the given input descriptors.
  // Listed languages will be used as Accept-Language header.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  // "most relevant" languages, as set in initial_locale in VPD, will be first
  // in the list.
  static base::ListValue* GetAcceptLanguageList(
      const input_method::InputMethodDescriptors& descriptors);

  // Gets the list of UI languages with the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  // "most relevant" languages, as set in initial_locale in VPD, will be first
  // in the list.
  // An entry with "code" attribute set to kVendorOtherLanguagesListDivider is
  // used as a divider to separate "most relevant" languages against other.
  static base::ListValue* GetUILanguageList(
      const input_method::InputMethodDescriptors& descriptors);

  // Converts input method descriptors to the list of input methods.
  // The return value will look like:
  // [{'id': '_ext_ime_nejguenhnsnjnwychcnsdsdjketest',
  //   'displayName': 'Sample IME'},  ...]
  static base::ListValue* ConvertInputMethodDescriptosToIMEList(
      const input_method::InputMethodDescriptors& descriptors);

 private:
  // LanguageOptionsHandlerCommon implementation.
  virtual base::string16 GetProductName() OVERRIDE;
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

  // ComponentExtensionIMEManager::Observer override.
  virtual void OnImeComponentExtensionInitialized() OVERRIDE;

  // Gets the list of languages with |descriptors| based on
  // |base_language_codes|.
  // |insert_divider| means to insert entry with "code" attribute set to
  // kVendorOtherLanguagesListDivider between "most relevant" languages and
  // other.
  static base::ListValue* GetLanguageListInternal(
      const input_method::InputMethodDescriptors& descriptors,
      const std::vector<std::string>& base_language_codes,
      bool insert_divider);

  // OptionsPageUIHandler implementation.
  virtual void InitializePage() OVERRIDE;

  // True if the component extension list was appended into input method list.
  bool composition_extension_appended_;

  // True if this page was initialized.
  bool is_page_initialized_;

  DISALLOW_COPY_AND_ASSIGN(CrosLanguageOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
