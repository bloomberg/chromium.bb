// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api_constants.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_preference_api_constants.h"
#include "chrome/browser/extensions/extension_preference_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/plugin_service.h"
#include "webkit/plugins/npapi/plugin_group.h"

using content::BrowserThread;
using content::PluginService;

namespace pref_helpers = extension_preference_helpers;
namespace pref_keys = extension_preference_api_constants;

namespace {

const std::vector<webkit::npapi::PluginGroup>* g_testing_plugin_groups_;

}  // namespace

namespace extensions {

namespace helpers = content_settings_helpers;
namespace keys = content_settings_api_constants;

bool ClearContentSettingsFunction::RunImpl() {
  std::string content_type_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &content_type_str));
  ContentSettingsType content_type =
      helpers::StringToContentSettingsType(content_type_str);
  EXTENSION_FUNCTION_VALIDATE(content_type != CONTENT_SETTINGS_TYPE_DEFAULT);

  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (details->HasKey(pref_keys::kScopeKey)) {
    std::string scope_str;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(pref_keys::kScopeKey,
                                                   &scope_str));

    EXTENSION_FUNCTION_VALIDATE(pref_helpers::StringToScope(scope_str, &scope));
    EXTENSION_FUNCTION_VALIDATE(
        scope != kExtensionPrefsScopeIncognitoPersistent);
  }

  bool incognito = (scope == kExtensionPrefsScopeIncognitoPersistent ||
                    scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // We don't check incognito permissions here, as an extension should be
    // always allowed to clear its own settings.
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (profile()->IsOffTheRecord()) {
      error_ = keys::kIncognitoContextError;
      return false;
    }
  }

  ContentSettingsStore* store =
      profile_->GetExtensionService()->GetContentSettingsStore();
  store->ClearContentSettingsForExtension(extension_id(), scope);

  return true;
}

bool GetContentSettingFunction::RunImpl() {
  std::string content_type_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &content_type_str));
  ContentSettingsType content_type =
      helpers::StringToContentSettingsType(content_type_str);
  EXTENSION_FUNCTION_VALIDATE(content_type != CONTENT_SETTINGS_TYPE_DEFAULT);

  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  std::string primary_url_spec;
  EXTENSION_FUNCTION_VALIDATE(
      details->GetString(keys::kPrimaryUrlKey, &primary_url_spec));
  GURL primary_url(primary_url_spec);
  if (!primary_url.is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                     primary_url_spec);
    return false;
  }

  GURL secondary_url(primary_url);
  std::string secondary_url_spec;
  if (details->GetString(keys::kSecondaryUrlKey, &secondary_url_spec)) {
    secondary_url = GURL(secondary_url_spec);
    if (!secondary_url.is_valid()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                       secondary_url_spec);
      return false;
    }
  }

  std::string resource_identifier;
  if (details->HasKey(keys::kResourceIdentifierKey)) {
    DictionaryValue* resource_identifier_dict = NULL;
    EXTENSION_FUNCTION_VALIDATE(
        details->GetDictionary(keys::kResourceIdentifierKey,
                               &resource_identifier_dict));
    EXTENSION_FUNCTION_VALIDATE(
        resource_identifier_dict->GetString(keys::kIdKey,
                                            &resource_identifier));
  }

  bool incognito = false;
  if (details->HasKey(pref_keys::kIncognitoKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        details->GetBoolean(pref_keys::kIncognitoKey, &incognito));
  }
  if (incognito && !include_incognito()) {
    error_ = pref_keys::kIncognitoErrorMessage;
    return false;
  }

  HostContentSettingsMap* map;
  CookieSettings* cookie_settings;
  if (incognito) {
    if (!profile()->HasOffTheRecordProfile()) {
      // TODO(bauerb): Allow reading incognito content settings
      // outside of an incognito session.
      error_ = keys::kIncognitoSessionOnlyError;
      return false;
    }
    map = profile()->GetOffTheRecordProfile()->GetHostContentSettingsMap();
    cookie_settings = CookieSettings::Factory::GetForProfile(
        profile()->GetOffTheRecordProfile());
  } else {
    map = profile()->GetHostContentSettingsMap();
    cookie_settings = CookieSettings::Factory::GetForProfile(profile());
  }

  ContentSetting setting;
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
    // TODO(jochen): Do we return the value for setting or for reading cookies?
    bool setting_cookie = false;
    setting = cookie_settings->GetCookieSetting(primary_url, secondary_url,
                                                setting_cookie, NULL);
  } else {
    setting = map->GetContentSetting(primary_url, secondary_url, content_type,
                                     resource_identifier);
  }

  DictionaryValue* result = new DictionaryValue();
  result->SetString(keys::kContentSettingKey,
                    helpers::ContentSettingToString(setting));

  result_.reset(result);

  return true;
}

bool SetContentSettingFunction::RunImpl() {
  std::string content_type_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &content_type_str));
  ContentSettingsType content_type =
      helpers::StringToContentSettingsType(content_type_str);
  EXTENSION_FUNCTION_VALIDATE(content_type != CONTENT_SETTINGS_TYPE_DEFAULT);

  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  std::string primary_pattern_str;
  EXTENSION_FUNCTION_VALIDATE(
      details->GetString(keys::kPrimaryPatternKey, &primary_pattern_str));
  std::string primary_error;
  ContentSettingsPattern primary_pattern =
      helpers::ParseExtensionPattern(primary_pattern_str, &primary_error);
  if (!primary_pattern.IsValid()) {
    error_ = primary_error;
    return false;
  }

  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  std::string secondary_pattern_str;
  if (details->GetString(keys::kSecondaryPatternKey, &secondary_pattern_str)) {
    std::string secondary_error;
    secondary_pattern =
        helpers::ParseExtensionPattern(secondary_pattern_str, &secondary_error);
    if (!secondary_pattern.IsValid()) {
      error_ = secondary_error;
      return false;
    }
  }

  std::string resource_identifier;
  if (details->HasKey(keys::kResourceIdentifierKey)) {
    DictionaryValue* resource_identifier_dict = NULL;
    EXTENSION_FUNCTION_VALIDATE(
        details->GetDictionary(keys::kResourceIdentifierKey,
                               &resource_identifier_dict));
    EXTENSION_FUNCTION_VALIDATE(
        resource_identifier_dict->GetString(keys::kIdKey,
                                            &resource_identifier));
  }

  std::string setting_str;
  EXTENSION_FUNCTION_VALIDATE(
      details->GetString(keys::kContentSettingKey, &setting_str));
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  EXTENSION_FUNCTION_VALIDATE(
      helpers::StringToContentSetting(setting_str, &setting));
  EXTENSION_FUNCTION_VALIDATE(
      HostContentSettingsMap::IsSettingAllowedForType(setting, content_type));

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (details->HasKey(pref_keys::kScopeKey)) {
    std::string scope_str;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(pref_keys::kScopeKey,
                                                   &scope_str));

    EXTENSION_FUNCTION_VALIDATE(pref_helpers::StringToScope(scope_str, &scope));
    EXTENSION_FUNCTION_VALIDATE(
        scope != kExtensionPrefsScopeIncognitoPersistent);
  }

  bool incognito = (scope == kExtensionPrefsScopeIncognitoPersistent ||
                    scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // Regular profiles can't access incognito unless include_incognito is true.
    if (!profile()->IsOffTheRecord() && !include_incognito()) {
      error_ = pref_keys::kIncognitoErrorMessage;
      return false;
    }
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (profile()->IsOffTheRecord()) {
      error_ = keys::kIncognitoContextError;
      return false;
    }
  }

  if (scope == kExtensionPrefsScopeIncognitoSessionOnly &&
      !profile_->HasOffTheRecordProfile()) {
    error_ = pref_keys::kIncognitoSessionOnlyErrorMessage;
    return false;
  }

  ContentSettingsStore* store =
      profile_->GetExtensionService()->GetContentSettingsStore();
  store->SetExtensionContentSetting(extension_id(), primary_pattern,
                                    secondary_pattern, content_type,
                                    resource_identifier, setting, scope);
  return true;
}

bool GetResourceIdentifiersFunction::RunImpl() {
  std::string content_type_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &content_type_str));
  ContentSettingsType content_type =
      helpers::StringToContentSettingsType(content_type_str);
  EXTENSION_FUNCTION_VALIDATE(content_type != CONTENT_SETTINGS_TYPE_DEFAULT);

  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    if (g_testing_plugin_groups_) {
      OnGotPluginGroups(*g_testing_plugin_groups_);
    } else {
      PluginService::GetInstance()->GetPluginGroups(
          base::Bind(&GetResourceIdentifiersFunction::OnGotPluginGroups, this));
    }
  } else {
    SendResponse(true);
  }

  return true;
}

void GetResourceIdentifiersFunction::OnGotPluginGroups(
    const std::vector<webkit::npapi::PluginGroup>& groups) {
  ListValue* list = new ListValue();
  for (std::vector<webkit::npapi::PluginGroup>::const_iterator it =
          groups.begin();
       it != groups.end(); ++it) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString(keys::kIdKey, it->identifier());
    dict->SetString(keys::kDescriptionKey, it->GetGroupName());
    list->Append(dict);
  }
  result_.reset(list);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          &GetResourceIdentifiersFunction::SendResponse, this, true));
}

// static
void GetResourceIdentifiersFunction::SetPluginGroupsForTesting(
    const std::vector<webkit::npapi::PluginGroup>* plugin_groups) {
  g_testing_plugin_groups_ = plugin_groups;
}

}  // namespace extensions
