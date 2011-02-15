// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_HANGUL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_HANGUL_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options/options_ui.h"

class DictionaryValue;
class ListValue;

namespace chromeos {

// Hangul options page UI handler.
class LanguageHangulOptionsHandler : public OptionsPageUIHandler {
 public:
  LanguageHangulOptionsHandler();
  virtual ~LanguageHangulOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  // Returns the list of hangul keyboards.
  static ListValue* GetKeyboardLayoutList();

  DISALLOW_COPY_AND_ASSIGN(LanguageHangulOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_HANGUL_OPTIONS_HANDLER_H_
