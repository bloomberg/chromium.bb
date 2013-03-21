// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class ListValue;
}

namespace options {

// Font settings overlay page UI handler.
class FontSettingsHandler : public OptionsPageUIHandler {
 public:
  FontSettingsHandler();
  virtual ~FontSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void HandleFetchFontsData(const ListValue* args);

  void FontsListHasLoaded(scoped_ptr<base::ListValue> list);

  void SetUpStandardFontSample();
  void SetUpSerifFontSample();
  void SetUpSansSerifFontSample();
  void SetUpFixedFontSample();
  void SetUpMinimumFontSample();
  void OnWebKitDefaultFontSizeChanged();

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

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_HANDLER_H_
