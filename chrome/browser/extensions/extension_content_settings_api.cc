// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_content_settings_api.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_content_settings_api_constants.h"
#include "chrome/browser/extensions/extension_content_settings_helpers.h"
#include "chrome/browser/extensions/extension_content_settings_store.h"
#include "chrome/browser/extensions/extension_preference_api_constants.h"
#include "chrome/browser/extensions/extension_preference_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace helpers = extension_content_settings_helpers;
namespace keys = extension_content_settings_api_constants;
namespace pref_helpers = extension_preference_helpers;
namespace pref_keys = extension_preference_api_constants;

namespace {

webkit::npapi::PluginList* g_plugin_list = NULL;

}  // namespace

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

  ExtensionContentSettingsStore* store =
      profile_->GetExtensionService()->GetExtensionContentSettingsStore();
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

  std::string embedded_url_spec;
  EXTENSION_FUNCTION_VALIDATE(
      details->GetString(keys::kEmbeddedUrlKey, &embedded_url_spec));
  GURL embedded_url(embedded_url_spec);
  if (!embedded_url.is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                     embedded_url_spec);
    return false;
  }

  std::string top_level_url_spec;
  EXTENSION_FUNCTION_VALIDATE(
      details->GetString(keys::kTopLevelUrlKey, &top_level_url_spec));
  GURL top_level_url(top_level_url_spec);
  if (!top_level_url.is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                     top_level_url_spec);
    return false;
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
  if (incognito) {
    if (!profile()->HasOffTheRecordProfile()) {
      // TODO(bauerb): Allow reading incognito content settings
      // outside of an incognito session.
      error_ = keys::kIncognitoSessionOnlyError;
      return false;
    }
    map = profile()->GetOffTheRecordProfile()->GetHostContentSettingsMap();
  } else {
    map = profile()->GetHostContentSettingsMap();
  }

  ContentSetting setting;
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
    // TODO(jochen): Do we return the value for setting or for reading cookies?
    bool setting_cookie = false;
    setting = map->GetCookieContentSetting(embedded_url, top_level_url,
                                           setting_cookie);
  } else {
    setting = map->GetContentSetting(embedded_url, top_level_url, content_type,
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

  std::string top_level_pattern_str;
  std::string top_level_error;
  EXTENSION_FUNCTION_VALIDATE(
      details->GetString(keys::kTopLevelPatternKey, &top_level_pattern_str));
  ContentSettingsPattern top_level_pattern =
      helpers::ParseExtensionPattern(top_level_pattern_str, &top_level_error);
  if (!top_level_pattern.IsValid()) {
    error_ = top_level_error;
    return false;
  }

  std::string embedded_pattern_str;
  std::string embedded_error;
  EXTENSION_FUNCTION_VALIDATE(
      details->GetString(keys::kEmbeddedPatternKey, &embedded_pattern_str));
  ContentSettingsPattern embedded_pattern =
      helpers::ParseExtensionPattern(embedded_pattern_str, &embedded_error);
  if (!embedded_pattern.IsValid()) {
    error_ = embedded_error;
    return false;
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

  ExtensionContentSettingsStore* store =
      profile_->GetExtensionService()->GetExtensionContentSettingsStore();
  store->SetExtensionContentSetting(extension_id(), top_level_pattern,
                                    embedded_pattern, content_type,
                                    resource_identifier, setting, scope);
  return true;
}

bool GetResourceIdentifiersFunction::RunImpl() {
  std::string content_type_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &content_type_str));
  ContentSettingsType content_type =
      helpers::StringToContentSettingsType(content_type_str);
  EXTENSION_FUNCTION_VALIDATE(content_type != CONTENT_SETTINGS_TYPE_DEFAULT);

  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableResourceContentSettings)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&GetResourceIdentifiersFunction::GetPluginsOnFileThread,
                   this));
  } else {
    SendResponse(true);
  }

  return true;
}

void GetResourceIdentifiersFunction::GetPluginsOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  webkit::npapi::PluginList* plugin_list = g_plugin_list;
  if (!plugin_list) {
    plugin_list = webkit::npapi::PluginList::Singleton();
  }

  std::vector<webkit::npapi::PluginGroup> groups;
  plugin_list->GetPluginGroups(true, &groups);

  ListValue* list = new ListValue();
  for (std::vector<webkit::npapi::PluginGroup>::iterator it = groups.begin();
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
void GetResourceIdentifiersFunction::SetPluginListForTesting(
    webkit::npapi::PluginList* plugin_list) {
  g_plugin_list = plugin_list;
}
