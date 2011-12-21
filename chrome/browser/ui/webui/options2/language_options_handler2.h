// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_LANGUAGE_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_LANGUAGE_OPTIONS_HANDLER2_H_
#pragma once

#include "chrome/browser/ui/webui/options2/language_options_handler_common2.h"

namespace options2 {

// Language options UI page handler for non-Chrome OS platforms.  For Chrome OS,
// see chromeos::CrosLanguageOptionsHandler.
class LanguageOptionsHandler : public LanguageOptionsHandlerCommon {
 public:
  LanguageOptionsHandler();
  virtual ~LanguageOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // The following static method is public for ease of testing.

  // Gets the list of languages from the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  static base::ListValue* GetLanguageList();

 private:
  // LanguageOptionsHandlerCommon implementation.
  virtual string16 GetProductName() OVERRIDE;
  virtual void SetApplicationLocale(const std::string& language_code) OVERRIDE;

  // Called when the restart button is clicked.
  void RestartCallback(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_LANGUAGE_OPTIONS_HANDLER2_H_
