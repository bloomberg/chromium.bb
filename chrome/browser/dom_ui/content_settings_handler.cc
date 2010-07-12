// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/content_settings_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// The list of content settings types that we display in the tabbed options
// under the Content Settings page. This should be filled in as more pages
// are added.
ContentSettingsType kContentSettingsTypes[] = {
  CONTENT_SETTINGS_TYPE_COOKIES,
  CONTENT_SETTINGS_TYPE_IMAGES,
};

std::wstring ContentSettingsTypeToGroupName(ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return L"cookies";
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return L"images";

    default:
      NOTREACHED();
      return L"";
  }
}

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  if (name == "cookies")
    return CONTENT_SETTINGS_TYPE_COOKIES;
  if (name == "images")
    return CONTENT_SETTINGS_TYPE_IMAGES;

  NOTREACHED();
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

std::string ContentSettingToString(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return "allow";
    case CONTENT_SETTING_ASK:
      return "ask";
    case CONTENT_SETTING_BLOCK:
      return "block";

    default:
      NOTREACHED();
      return "";
  }
}

ContentSetting ContentSettingFromString(const std::string& name) {
  if (name == "allow")
    return CONTENT_SETTING_ALLOW;
  if (name == "ask")
    return CONTENT_SETTING_ASK;
  if (name == "block")
    return CONTENT_SETTING_BLOCK;

  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

}  // namespace

ContentSettingsHandler::ContentSettingsHandler() {
}

ContentSettingsHandler::~ContentSettingsHandler() {
}

void ContentSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString(L"content_exceptions",
      l10n_util::GetString(IDS_COOKIES_EXCEPTIONS_BUTTON));
  localized_strings->SetString(L"contentSettingsPage",
      l10n_util::GetString(IDS_CONTENT_SETTINGS_TITLE));

  // Cookies filter.
  localized_strings->SetString(L"cookies_tab_label",
      l10n_util::GetString(IDS_COOKIES_TAB_LABEL));
  localized_strings->SetString(L"cookies_modify",
      l10n_util::GetString(IDS_MODIFY_COOKIE_STORING_LABEL));
  localized_strings->SetString(L"cookies_allow",
      l10n_util::GetString(IDS_COOKIES_ALLOW_RADIO));
  localized_strings->SetString(L"cookies_ask",
      l10n_util::GetString(IDS_COOKIES_ASK_EVERY_TIME_RADIO));
  localized_strings->SetString(L"cookies_block",
      l10n_util::GetString(IDS_COOKIES_BLOCK_RADIO));
  localized_strings->SetString(L"cookies_block_3rd_party",
      l10n_util::GetString(IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX));
  localized_strings->SetString(L"cookies_clear_on_exit",
      l10n_util::GetString(IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX));
  localized_strings->SetString(L"cookies_show_cookies",
      l10n_util::GetString(IDS_COOKIES_SHOW_COOKIES_BUTTON));
  localized_strings->SetString(L"flash_storage_settings",
      l10n_util::GetString(IDS_FLASH_STORAGE_SETTINGS));
  localized_strings->SetString(L"flash_storage_url",
      l10n_util::GetString(IDS_FLASH_STORAGE_URL));

  // Image filter.
  localized_strings->SetString(L"images_tab_label",
      l10n_util::GetString(IDS_IMAGES_TAB_LABEL));
  localized_strings->SetString(L"images_setting",
      l10n_util::GetString(IDS_IMAGES_SETTING_LABEL));
  localized_strings->SetString(L"images_allow",
      l10n_util::GetString(IDS_IMAGES_LOAD_RADIO));
  localized_strings->SetString(L"images_block",
      l10n_util::GetString(IDS_IMAGES_NOLOAD_RADIO));
}

void ContentSettingsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getContentFilterSettings",
      NewCallback(this,
                  &ContentSettingsHandler::GetContentFilterSettings));
  dom_ui_->RegisterMessageCallback("setContentFilter",
      NewCallback(this,
                  &ContentSettingsHandler::SetContentFilter));
  dom_ui_->RegisterMessageCallback("setAllowThirdPartyCookies",
      NewCallback(this,
                  &ContentSettingsHandler::SetAllowThirdPartyCookies));
}

void ContentSettingsHandler::GetContentFilterSettings(const Value* value) {
  // We send a list of the <input> IDs that should be checked.
  DictionaryValue dict_value;

  const HostContentSettingsMap* settings_map =
      dom_ui_->GetProfile()->GetHostContentSettingsMap();
  for (size_t i = 0; i < arraysize(kContentSettingsTypes); ++i) {
    ContentSettingsType type = kContentSettingsTypes[i];
    ContentSetting default_setting = settings_map->
        GetDefaultContentSetting(type);

    dict_value.SetString(ContentSettingsTypeToGroupName(type),
                         ContentSettingToString(default_setting));
  }

  dom_ui_->CallJavascriptFunction(
      L"ContentSettings.setInitialContentFilterSettingsValue", dict_value);

  scoped_ptr<Value> bool_value(Value::CreateBooleanValue(
      settings_map->BlockThirdPartyCookies()));
  dom_ui_->CallJavascriptFunction(
      L"ContentSettings.setBlockThirdPartyCookies", *bool_value.get());
}

void ContentSettingsHandler::SetContentFilter(const Value* value) {
  const ListValue* list_value = static_cast<const ListValue*>(value);
  DCHECK_EQ(2U, list_value->GetSize());
  std::string group, setting;
  if (!(list_value->GetString(0, &group) &&
        list_value->GetString(1, &setting))) {
    NOTREACHED();
    return;
  }

  dom_ui_->GetProfile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      ContentSettingsTypeFromGroupName(group),
      ContentSettingFromString(setting));
}

void ContentSettingsHandler::SetAllowThirdPartyCookies(const Value* value) {
  std::wstring allow = ExtractStringValue(value);

  dom_ui_->GetProfile()->GetHostContentSettingsMap()->SetBlockThirdPartyCookies(
      allow == L"true");
}
