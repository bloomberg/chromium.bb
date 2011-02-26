// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/options/font_settings_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webui/options/dom_options_util.h"
#include "chrome/browser/webui/options/font_settings_utils.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

FontSettingsHandler::FontSettingsHandler() {
}

FontSettingsHandler::~FontSettingsHandler() {
}

void FontSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "fontSettingsStandard",
      IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_STANDARD_LABEL, true },
    { "fontSettingsFixedWidth",
      IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_FIXED_WIDTH_LABEL, true },
    { "fontSettingsMinimumSize",
      IDS_FONT_LANGUAGE_SETTING_MINIMUM_FONT_SIZE_TITLE },
    { "fontSettingsEncoding",
      IDS_FONT_LANGUAGE_SETTING_FONT_SUB_DIALOG_ENCODING_TITLE },
    { "fontSettingsSizeLabel",
      IDS_FONT_LANGUAGE_SETTING_FONT_SIZE_SELECTOR_LABEL },
    { "fontSettingsSizeTiny",
      IDS_FONT_LANGUAGE_SETTING_FONT_SIZE_TINY },
    { "fontSettingsSizeHuge",
      IDS_FONT_LANGUAGE_SETTING_FONT_SIZE_HUGE },
    { "fontSettingsEncodingLabel",
      IDS_FONT_LANGUAGE_SETTING_FONT_DEFAULT_ENCODING_SELECTOR_LABEL },
    { "fontSettingsLoremIpsum",
      IDS_FONT_LANGUAGE_SETTING_LOREM_IPSUM },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "fontSettingsPage",
                IDS_FONT_LANGUAGE_SETTING_FONT_TAB_TITLE);

  // Fonts
  ListValue* font_list = FontSettingsUtilities::GetFontsList();
  if (font_list) localized_strings->Set("fontSettingsFontList", font_list);

  // Font sizes
  int font_sizes[] = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26,
                       28, 30, 32, 34, 36, 40, 44, 48, 56, 64, 72 };
  int count = arraysize(font_sizes);
  ListValue* font_size_list = new ListValue;
  for (int i = 0; i < count; i++) {
    ListValue* option = new ListValue();
    option->Append(Value::CreateIntegerValue(font_sizes[i]));
    option->Append(Value::CreateStringValue(base::IntToString(font_sizes[i])));
    font_size_list->Append(option);
  }
  localized_strings->Set("fontSettingsFontSizeList", font_size_list);

  // Miniumum font size
  int minimum_font_sizes[] = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22,
                               24 };
  count = arraysize(minimum_font_sizes);
  ListValue* minimum_font_size_list = new ListValue;
  ListValue* default_option = new ListValue();
  default_option->Append(Value::CreateIntegerValue(0));
  default_option->Append(Value::CreateStringValue(
      l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_NO_MINIMUM_FONT_SIZE_LABEL)));
  minimum_font_size_list->Append(default_option);
  for (int i = 0; i < count; i++) {
    ListValue* option = new ListValue();
    option->Append(Value::CreateIntegerValue(minimum_font_sizes[i]));
    option->Append(
        Value::CreateStringValue(base::IntToString(minimum_font_sizes[i])));
    minimum_font_size_list->Append(option);
  }
  localized_strings->Set("fontSettingsMinimumFontSizeList",
                         minimum_font_size_list);

  // Encodings
  count = CharacterEncoding::GetSupportCanonicalEncodingCount();
  ListValue* encoding_list = new ListValue;
  for (int i = 0; i < count; ++i) {
    int cmd_id = CharacterEncoding::GetEncodingCommandIdByIndex(i);
    std::string encoding =
        CharacterEncoding::GetCanonicalEncodingNameByCommandId(cmd_id);
    string16 name =
        CharacterEncoding::GetCanonicalEncodingDisplayNameByCommandId(cmd_id);

    ListValue* option = new ListValue();
    option->Append(Value::CreateStringValue(encoding));
    option->Append(Value::CreateStringValue(name));
    encoding_list->Append(option);
  }
  localized_strings->Set("fontSettingsEncodingList", encoding_list);
}

void FontSettingsHandler::Initialize() {
  SetupSerifFontSample();
  SetupMinimumFontSample();
  SetupFixedFontSample();
}

WebUIMessageHandler* FontSettingsHandler::Attach(WebUI* web_ui) {
  // Call through to superclass.
  WebUIMessageHandler* handler = OptionsPageUIHandler::Attach(web_ui);

  // Perform validation for saved fonts.
  DCHECK(web_ui_);
  PrefService* pref_service = web_ui_->GetProfile()->GetPrefs();
  FontSettingsUtilities::ValidateSavedFonts(pref_service);

  // Register for preferences that we need to observe manually.
  serif_font_.Init(prefs::kWebKitSerifFontFamily, pref_service, this);
  fixed_font_.Init(prefs::kWebKitFixedFontFamily, pref_service, this);
  default_font_size_.Init(prefs::kWebKitDefaultFontSize, pref_service, this);
  default_fixed_font_size_.Init(prefs::kWebKitDefaultFixedFontSize,
                                pref_service, this);
  minimum_font_size_.Init(prefs::kWebKitMinimumFontSize, pref_service, this);

  // Return result from the superclass.
  return handler;
}

void FontSettingsHandler::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::string* pref_name = Details<std::string>(details).ptr();
    if (*pref_name == prefs::kWebKitSerifFontFamily ||
        *pref_name == prefs::kWebKitDefaultFontSize) {
      SetupSerifFontSample();
    } else if (*pref_name == prefs::kWebKitFixedFontFamily ||
               *pref_name == prefs::kWebKitDefaultFixedFontSize) {
      SetupFixedFontSample();
    } else if (*pref_name == prefs::kWebKitMinimumFontSize) {
      SetupMinimumFontSample();
    }
  }
}

void FontSettingsHandler::SetupSerifFontSample() {
  DCHECK(web_ui_);
  StringValue font_value(serif_font_.GetValue());
  FundamentalValue size_value(default_font_size_.GetValue());
  web_ui_->CallJavascriptFunction(
      L"FontSettings.setupSerifFontSample", font_value, size_value);
}

void FontSettingsHandler::SetupFixedFontSample() {
  DCHECK(web_ui_);
  StringValue font_value(fixed_font_.GetValue());
  FundamentalValue size_value(default_fixed_font_size_.GetValue());
  web_ui_->CallJavascriptFunction(
      L"FontSettings.setupFixedFontSample", font_value, size_value);
}

void FontSettingsHandler::SetupMinimumFontSample() {
  DCHECK(web_ui_);
  FundamentalValue size_value(minimum_font_size_.GetValue());
  web_ui_->CallJavascriptFunction(
      L"FontSettings.setupMinimumFontSample", size_value);
}
