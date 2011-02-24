// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_CUSTOMIZE_MODIFIER_KEYS_HANDLER_H_  // NOLINT
#define CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_CUSTOMIZE_MODIFIER_KEYS_HANDLER_H_  // NOLINT
#pragma once

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/webui/options/options_ui.h"

namespace chromeos {

// Customize modifier keys overlay page UI handler.
class LanguageCustomizeModifierKeysHandler : public OptionsPageUIHandler {
 public:
  LanguageCustomizeModifierKeysHandler();
  virtual ~LanguageCustomizeModifierKeysHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageCustomizeModifierKeysHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LANGUAGE_CUSTOMIZE_MODIFIER_KEYS_HANDLER_H_  // NOLINT
