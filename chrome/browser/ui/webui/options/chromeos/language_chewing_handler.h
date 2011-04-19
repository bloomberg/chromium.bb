// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_LANGUAGE_CHEWING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_LANGUAGE_CHEWING_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class DictionaryValue;

namespace chromeos {

// Chewing options page UI handler.
class LanguageChewingHandler : public OptionsPageUIHandler {
 public:
  LanguageChewingHandler();
  virtual ~LanguageChewingHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageChewingHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_LANGUAGE_CHEWING_HANDLER_H_
