// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_LANGUAGE_CUSTOMIZE_MODIFIER_KEYS_HANDLER2_H_  // NOLINT
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_LANGUAGE_CUSTOMIZE_MODIFIER_KEYS_HANDLER2_H_  // NOLINT
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"

namespace chromeos {
namespace options2 {

// Customize modifier keys overlay page UI handler.
class LanguageCustomizeModifierKeysHandler
    : public ::options2::OptionsPageUIHandler {
 public:
  LanguageCustomizeModifierKeysHandler();
  virtual ~LanguageCustomizeModifierKeysHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageCustomizeModifierKeysHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_LANGUAGE_CUSTOMIZE_MODIFIER_KEYS_HANDLER2_H_  // NOLINT
