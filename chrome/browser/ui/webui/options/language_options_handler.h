// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_

#include "chrome/browser/ui/webui/options/language_options_handler_common.h"

namespace options {

// Language options UI page handler for non-Chrome OS platforms.  For Chrome OS,
// see chromeos::CrosLanguageOptionsHandler.
class LanguageOptionsHandler : public LanguageOptionsHandlerCommon {
 public:
  LanguageOptionsHandler();
  ~LanguageOptionsHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // The following static method is public for ease of testing.

  // Gets the list of languages from the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  static base::ListValue* GetLanguageList();

 private:
  // LanguageOptionsHandlerCommon implementation.
  base::string16 GetProductName() override;
  void SetApplicationLocale(const std::string& language_code) override;

  // Called when the restart button is clicked.
  void RestartCallback(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_
