// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/browser/prefs/pref_member.h"

// Font settings overlay page UI handler.
class FontSettingsHandler : public OptionsPageUIHandler {
 public:
  FontSettingsHandler();
  virtual ~FontSettingsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void SetupSerifFontPreview();
  void SetupSansSerifFontPreview();
  void SetupFixedFontPreview();

  StringPrefMember serif_font_;
  StringPrefMember sans_serif_font_;
  StringPrefMember fixed_font_;
  IntegerPrefMember default_font_size_;
  IntegerPrefMember default_fixed_font_size_;

  DISALLOW_COPY_AND_ASSIGN(FontSettingsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_FONT_SETTINGS_HANDLER_H_
