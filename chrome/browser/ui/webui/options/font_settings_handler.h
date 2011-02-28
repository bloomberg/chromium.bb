// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#pragma once

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options/font_settings_fonts_list_loader.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

// Font settings overlay page UI handler.
class FontSettingsHandler : public OptionsPageUIHandler,
                            public FontSettingsFontsListLoader::Observer {
 public:
  FontSettingsHandler();
  virtual ~FontSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // FontSettingsFontsListLoader::Observer implementation.
  virtual void FontsListHasLoaded();

 private:
  void HandleFetchFontsData(const ListValue* args);

  void SetupSerifFontSample();
  void SetupFixedFontSample();
  void SetupMinimumFontSample();

  StringPrefMember serif_font_;
  StringPrefMember fixed_font_;
  StringPrefMember font_encoding_;
  IntegerPrefMember default_font_size_;
  IntegerPrefMember default_fixed_font_size_;
  IntegerPrefMember minimum_font_size_;

  scoped_refptr<FontSettingsFontsListLoader> fonts_list_loader_;

  DISALLOW_COPY_AND_ASSIGN(FontSettingsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
