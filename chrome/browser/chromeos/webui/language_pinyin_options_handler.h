// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_PINYIN_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_PINYIN_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/options/options_ui.h"

class DictionaryValue;

namespace chromeos {

// Pinyin options page UI handler.
class LanguagePinyinOptionsHandler : public OptionsPageUIHandler {
 public:
  LanguagePinyinOptionsHandler();
  virtual ~LanguagePinyinOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguagePinyinOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_PINYIN_OPTIONS_HANDLER_H_
