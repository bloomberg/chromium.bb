// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"
#include "ui/base/text/bytes_formatting.h"

namespace settings {

namespace {

const char kAppName[] = "appName";
const char kAppId[] = "appId";

// Return an appropriate API Permission ID for the given string name.
extensions::APIPermission::APIPermission::ID APIPermissionFromGroupName(
    std::string type) {
  // Once there are more than two groups to consider, this should be changed to
  // something better than if's.

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      CONTENT_SETTINGS_TYPE_GEOLOCATION)
    return extensions::APIPermission::APIPermission::kGeolocation;

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    return extensions::APIPermission::APIPermission::kNotifications;

  return extensions::APIPermission::APIPermission::kInvalid;
}

// Add an "Allow"-entry to the list of |exceptions| for a |url_pattern| from
// the web extent of a hosted |app|.
void AddExceptionForHostedApp(const std::string& url_pattern,
    const extensions::Extension& app, base::ListValue* exceptions) {
  std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW);
  DCHECK(!setting_string.empty());

  exception->SetString(site_settings::kSetting, setting_string);
  exception->SetString(site_settings::kOrigin, url_pattern);
  exception->SetString(site_settings::kEmbeddingOrigin, url_pattern);
  exception->SetString(site_settings::kSource, "HostedApp");
  exception->SetString(kAppName, app.name());
  exception->SetString(kAppId, app.id());
  exceptions->Append(std::move(exception));
}

// Asks the |profile| for hosted apps which have the |permission| set, and
// adds their web extent and launch URL to the |exceptions| list.
void AddExceptionsGrantedByHostedApps(content::BrowserContext* context,
    extensions::APIPermission::APIPermission::ID permission,
    base::ListValue* exceptions) {
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(context)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator extension = extensions.begin();
       extension != extensions.end(); ++extension) {
    if (!(*extension)->is_hosted_app() ||
        !(*extension)->permissions_data()->HasAPIPermission(permission))
      continue;

    extensions::URLPatternSet web_extent = (*extension)->web_extent();
    // Add patterns from web extent.
    for (extensions::URLPatternSet::const_iterator pattern = web_extent.begin();
         pattern != web_extent.end(); ++pattern) {
      std::string url_pattern = pattern->GetAsString();
      AddExceptionForHostedApp(url_pattern, *extension->get(), exceptions);
    }
    // Retrieve the launch URL.
    GURL launch_url =
        extensions::AppLaunchInfo::GetLaunchWebURL(extension->get());
    // Skip adding the launch URL if it is part of the web extent.
    if (web_extent.MatchesURL(launch_url))
      continue;
    AddExceptionForHostedApp(launch_url.spec(), *extension->get(), exceptions);
  }
}

}  // namespace


SiteSettingsHandler::SiteSettingsHandler(Profile* profile)
    : profile_(profile), observer_(this) {
}

SiteSettingsHandler::~SiteSettingsHandler() {
}

void SiteSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchUsageTotal",
      base::Bind(&SiteSettingsHandler::HandleFetchUsageTotal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearUsage",
      base::Bind(&SiteSettingsHandler::HandleClearUsage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchUsbDevices",
      base::Bind(&SiteSettingsHandler::HandleFetchUsbDevices,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeUsbDevice",
      base::Bind(&SiteSettingsHandler::HandleRemoveUsbDevice,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDefaultValueForContentType",
      base::Bind(&SiteSettingsHandler::HandleSetDefaultValueForContentType,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDefaultValueForContentType",
      base::Bind(&SiteSettingsHandler::HandleGetDefaultValueForContentType,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExceptionList",
      base::Bind(&SiteSettingsHandler::HandleGetExceptionList,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetCategoryPermissionForOrigin",
      base::Bind(&SiteSettingsHandler::HandleResetCategoryPermissionForOrigin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setCategoryPermissionForOrigin",
      base::Bind(&SiteSettingsHandler::HandleSetCategoryPermissionForOrigin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isPatternValid",
      base::Bind(&SiteSettingsHandler::HandleIsPatternValid,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateIncognitoStatus",
      base::Bind(&SiteSettingsHandler::HandleUpdateIncognitoStatus,
                 base::Unretained(this)));
}

void SiteSettingsHandler::OnJavascriptAllowed() {
  observer_.Add(HostContentSettingsMapFactory::GetForProfile(profile_));
  if (profile_->HasOffTheRecordProfile()) {
    auto* map = HostContentSettingsMapFactory::GetForProfile(
        profile_->GetOffTheRecordProfile());
    if (!observer_.IsObserving(map))
      observer_.Add(map);
  }

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllSources());
}

void SiteSettingsHandler::OnJavascriptDisallowed() {
  observer_.RemoveAll();
  notification_registrar_.RemoveAll();
}

void SiteSettingsHandler::OnGetUsageInfo(
    const storage::UsageInfoEntries& entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& entry : entries) {
    if (entry.usage <= 0) continue;
    if (entry.host == usage_host_) {
      CallJavascriptFunction("settings.WebsiteUsagePrivateApi.returnUsageTotal",
                             base::StringValue(entry.host),
                             base::StringValue(ui::FormatBytes(entry.usage)),
                             base::FundamentalValue(entry.type));
      return;
    }
  }
}

void SiteSettingsHandler::OnUsageInfoCleared(storage::QuotaStatusCode code) {
  if (code == storage::kQuotaStatusOk) {
    CallJavascriptFunction("settings.WebsiteUsagePrivateApi.onUsageCleared",
                           base::StringValue(clearing_origin_));
  }
}

void SiteSettingsHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  if (!site_settings::HasRegisteredGroupName(content_type))
    return;

  if (primary_pattern.ToString().empty()) {
    CallJavascriptFunction(
        "cr.webUIListenerCallback",
        base::StringValue("contentSettingCategoryChanged"),
        base::StringValue(site_settings::ContentSettingsTypeToGroupName(
            content_type)));
  } else {
    CallJavascriptFunction(
        "cr.webUIListenerCallback",
        base::StringValue("contentSettingSitePermissionChanged"),
        base::StringValue(site_settings::ContentSettingsTypeToGroupName(
            content_type)),
        base::StringValue(primary_pattern.ToString()),
        base::StringValue(
            secondary_pattern == ContentSettingsPattern::Wildcard() ?
            "" : secondary_pattern.ToString()));
  }
}

void SiteSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile_->IsSameProfile(profile))
        break;
      SendIncognitoStatus(profile, /*was_destroyed=*/ true);

      HostContentSettingsMap* settings_map =
          HostContentSettingsMapFactory::GetForProfile(profile);
      if (profile->IsOffTheRecord() &&
          observer_.IsObserving(settings_map)) {
        observer_.Remove(settings_map);
      }

      break;
    }

    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile_->IsSameProfile(profile))
        break;
      SendIncognitoStatus(profile, /*was_destroyed=*/ false);

      observer_.Add(HostContentSettingsMapFactory::GetForProfile(profile));
      break;
    }
  }
}

void SiteSettingsHandler::HandleFetchUsageTotal(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string host;
  CHECK(args->GetString(0, &host));
  usage_host_ = host;

  scoped_refptr<StorageInfoFetcher> storage_info_fetcher
      = new StorageInfoFetcher(profile_);
  storage_info_fetcher->FetchStorageInfo(
      base::Bind(&SiteSettingsHandler::OnGetUsageInfo, base::Unretained(this)));
}

void SiteSettingsHandler::HandleClearUsage(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string origin;
  CHECK(args->GetString(0, &origin));
  std::string type;
  CHECK(args->GetString(1, &type));

  GURL url(origin);
  if (url.is_valid()) {
    clearing_origin_ = origin;

    // Start by clearing the storage data asynchronously.
    scoped_refptr<StorageInfoFetcher> storage_info_fetcher
        = new StorageInfoFetcher(profile_);
    storage_info_fetcher->ClearStorage(
        url.host(),
        static_cast<storage::StorageType>(static_cast<int>(
            site_settings::ContentSettingsTypeFromGroupName(type))),
        base::Bind(&SiteSettingsHandler::OnUsageInfoCleared,
            base::Unretained(this)));

    // Also clear the *local* storage data.
    scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper =
        new BrowsingDataLocalStorageHelper(profile_);
    local_storage_helper->DeleteOrigin(url);
  }
}

void SiteSettingsHandler::HandleFetchUsbDevices(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  base::ListValue exceptions;
  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(site_settings::kGroupTypeUsb);
  // TODO(finnur): Figure out whether incognito permissions are also needed.
  site_settings::GetChooserExceptionsFromProfile(
      profile_, false, *chooser_type, &exceptions);
  ResolveJavascriptCallback(*callback_id, exceptions);
}

void SiteSettingsHandler::HandleRemoveUsbDevice(const base::ListValue* args) {
  CHECK_EQ(3U, args->GetSize());

  std::string origin_string;
  CHECK(args->GetString(0, &origin_string));
  GURL requesting_origin(origin_string);
  CHECK(requesting_origin.is_valid());

  std::string embedding_origin_string;
  CHECK(args->GetString(1, &embedding_origin_string));
  GURL embedding_origin(embedding_origin_string);
  CHECK(embedding_origin.is_valid());

  const base::DictionaryValue* object = nullptr;
  CHECK(args->GetDictionary(2, &object));

  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(site_settings::kGroupTypeUsb);
  ChooserContextBase* chooser_context = chooser_type->get_context(profile_);
  chooser_context->RevokeObjectPermission(requesting_origin, embedding_origin,
                                          *object);
}

void SiteSettingsHandler::HandleSetDefaultValueForContentType(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string content_type;
  CHECK(args->GetString(0, &content_type));
  std::string setting;
  CHECK(args->GetString(1, &setting));
  ContentSetting default_setting;
  CHECK(content_settings::ContentSettingFromString(setting, &default_setting));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  map->SetDefaultContentSetting(
      static_cast<ContentSettingsType>(static_cast<int>(
          site_settings::ContentSettingsTypeFromGroupName(content_type))),
      default_setting);
}

void SiteSettingsHandler::HandleGetDefaultValueForContentType(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string type;
  CHECK(args->GetString(1, &type));

  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(
          site_settings::ContentSettingsTypeFromGroupName(type)));
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  std::string setting = content_settings::ContentSettingToString(
      map->GetDefaultContentSetting(content_type, nullptr));
  ResolveJavascriptCallback(*callback_id, base::StringValue(setting));
}

void SiteSettingsHandler::HandleGetExceptionList(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string type;
  CHECK(args->GetString(1, &type));
  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(
          site_settings::ContentSettingsTypeFromGroupName(type)));

  std::unique_ptr<base::ListValue> exceptions(new base::ListValue);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  AddExceptionsGrantedByHostedApps(profile_, APIPermissionFromGroupName(type),
      exceptions.get());
  site_settings::GetExceptionsFromHostContentSettingsMap(
      map, content_type, web_ui(), false, exceptions.get());

  if (profile_->HasOffTheRecordProfile()) {
    Profile* incognito = profile_->GetOffTheRecordProfile();
    map = HostContentSettingsMapFactory::GetForProfile(incognito);
    site_settings::GetExceptionsFromHostContentSettingsMap(
        map, content_type, web_ui(), true, exceptions.get());
  }

  ResolveJavascriptCallback(*callback_id, *exceptions.get());
}

void SiteSettingsHandler::HandleResetCategoryPermissionForOrigin(
    const base::ListValue* args) {
  CHECK_EQ(4U, args->GetSize());
  std::string primary_pattern;
  CHECK(args->GetString(0, &primary_pattern));
  std::string secondary_pattern;
  CHECK(args->GetString(1, &secondary_pattern));
  std::string type;
  CHECK(args->GetString(2, &type));
  bool incognito;
  CHECK(args->GetBoolean(3, &incognito));

  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(
          site_settings::ContentSettingsTypeFromGroupName(type)));

  Profile* profile = nullptr;
  if (incognito) {
    if (!profile_->HasOffTheRecordProfile())
      return;
    profile = profile_->GetOffTheRecordProfile();
  } else {
    profile = profile_;
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(primary_pattern),
      secondary_pattern.empty() ?
          ContentSettingsPattern::Wildcard() :
          ContentSettingsPattern::FromString(secondary_pattern),
      content_type, "", CONTENT_SETTING_DEFAULT);
}

void SiteSettingsHandler::HandleSetCategoryPermissionForOrigin(
    const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());
  std::string primary_pattern;
  CHECK(args->GetString(0, &primary_pattern));
  std::string secondary_pattern;
  CHECK(args->GetString(1, &secondary_pattern));
  std::string type;
  CHECK(args->GetString(2, &type));
  std::string value;
  CHECK(args->GetString(3, &value));
  bool incognito;
  CHECK(args->GetBoolean(4, &incognito));

  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(
          site_settings::ContentSettingsTypeFromGroupName(type)));
  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(value, &setting));

  Profile* profile = nullptr;
  if (incognito) {
    if (!profile_->HasOffTheRecordProfile())
      return;
    profile = profile_->GetOffTheRecordProfile();
  } else {
    profile = profile_;
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(primary_pattern),
      secondary_pattern.empty() ?
          ContentSettingsPattern::Wildcard() :
          ContentSettingsPattern::FromString(secondary_pattern),
      content_type, "", setting);
}

void SiteSettingsHandler::HandleIsPatternValid(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string pattern_string;
  CHECK(args->GetString(1, &pattern_string));

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(pattern_string);
  ResolveJavascriptCallback(
      *callback_id, base::FundamentalValue(pattern.IsValid()));
}

void SiteSettingsHandler::HandleUpdateIncognitoStatus(
    const base::ListValue* args) {
  AllowJavascript();
  SendIncognitoStatus(profile_, /*was_destroyed=*/ false);
}

void SiteSettingsHandler::SendIncognitoStatus(
    Profile* profile, bool was_destroyed) {
  if (!IsJavascriptAllowed())
    return;

  // When an incognito profile is destroyed, it sends out the destruction
  // message before destroying, so HasOffTheRecordProfile for profile_ won't
  // return false until after the profile actually been destroyed.
  bool incognito_enabled = profile_->HasOffTheRecordProfile() &&
      !(was_destroyed && profile == profile_->GetOffTheRecordProfile());

  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::StringValue("onIncognitoStatusChanged"),
                         base::FundamentalValue(incognito_enabled));
}

}  // namespace settings
