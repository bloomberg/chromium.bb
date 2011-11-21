// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#pragma once

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/browser/font_list_async.h"

// Font settings overlay page UI handler.
class FontSettingsHandler : public OptionsPageUIHandler {
 public:
  FontSettingsHandler();
  virtual ~FontSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void HandleFetchFontsData(const ListValue* args);

  void FontsListHasLoaded(scoped_refptr<content::FontListResult> list);

  void SetUpStandardFontSample();
  void SetUpSerifFontSample();
  void SetUpSansSerifFontSample();
  void SetUpFixedFontSample();
  void SetUpMinimumFontSample();

  StringPrefMember standard_font_;
  StringPrefMember serif_font_;
  StringPrefMember sans_serif_font_;
  StringPrefMember fixed_font_;
  StringPrefMember font_encoding_;
  IntegerPrefMember default_font_size_;
  IntegerPrefMember default_fixed_font_size_;
  IntegerPrefMember minimum_font_size_;

  DISALLOW_COPY_AND_ASSIGN(FontSettingsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
