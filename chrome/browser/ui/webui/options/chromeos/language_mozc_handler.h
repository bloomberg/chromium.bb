// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_LANGUAGE_MOZC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_LANGUAGE_MOZC_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/options/options_ui.h"

class DictionaryValue;

namespace chromeos {

// Mozc options page UI handler.
class LanguageMozcHandler : public OptionsPageUIHandler {
 public:
  LanguageMozcHandler();
  virtual ~LanguageMozcHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageMozcHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_LANGUAGE_MOZC_HANDLER_H_
