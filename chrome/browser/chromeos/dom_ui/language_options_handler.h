// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

class DictionaryValue;

// ChromeOS language options page UI handler.
class LanguageOptionsHandler : public OptionsPageUIHandler {
 public:
  LanguageOptionsHandler();
  virtual ~LanguageOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_HANDLER_H_
