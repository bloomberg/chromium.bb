// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/webui/options/language_options_handler_common.h"

// Language options UI page handler for non-Chrome OS platforms.  For Chrome OS,
// see chromeos::CrosLanguageOptionsHandler.
class LanguageOptionsHandler : public LanguageOptionsHandlerCommon {
 public:
  LanguageOptionsHandler();
  virtual ~LanguageOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // The following static method is public for ease of testing.

  // Gets the list of languages from the given input descriptors.
  // The return value will look like:
  // [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'},
  //  ...]
  static ListValue* GetLanguageList();

 private:
  // LanguageOptionsHandlerCommon implementation.
  virtual string16 GetProductName();
  virtual void SetApplicationLocale(std::string language_code);

  // Called when the restart button is clicked.
  void RestartCallback(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandler);
};

#endif  // CHROME_BROWSER_WEBUI_OPTIONS_LANGUAGE_OPTIONS_HANDLER_H_
