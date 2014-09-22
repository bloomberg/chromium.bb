// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/website_settings_handler.h"

#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/power/origin_power_map.h"
#include "components/power/origin_power_map_factory.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

using base::UserMetricsAction;
using power::OriginPowerMap;
using power::OriginPowerMapFactory;

namespace {

const char kBattery[] = "battery";
const int kHttpPort = 80;
const int kHttpsPort = 443;
const char kPreferencesSource[] = "preference";
const char kStorage[] = "storage";
const ContentSettingsType kValidTypes[] = {
    CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
    CONTENT_SETTINGS_TYPE_COOKIES,
    CONTENT_SETTINGS_TYPE_GEOLOCATION,
    CONTENT_SETTINGS_TYPE_IMAGES,
    CONTENT_SETTINGS_TYPE_JAVASCRIPT,
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM,
    CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTINGS_TYPE_POPUPS};
const size_t kValidTypesLength = arraysize(kValidTypes);

}  // namespace

namespace options {

WebsiteSettingsHandler::WebsiteSettingsHandler()
    : observer_(this),
      weak_ptr_factory_(this) {
}

WebsiteSettingsHandler::~WebsiteSettingsHandler() {
}

void WebsiteSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      {"websitesOptionsPageTabTitle", IDS_WEBSITE_SETTINGS_TITLE},
      {"websitesSettingsEditPage", IDS_WEBSITE_SETTINGS_EDIT_TITLE},
      {"websitesManage", IDS_WEBSITE_SETTINGS_MANAGE},
      {"websitesSearch", IDS_WEBSITE_SETTINGS_SEARCH_ORIGINS},
      {"websitesLabelLocation", IDS_WEBSITE_SETTINGS_TYPE_LOCATION},
      {"websitesLabelMediaStream", IDS_WEBSITE_SETTINGS_TYPE_MEDIASTREAM},
      {"websitesLabelNotifications", IDS_WEBSITE_SETTINGS_TYPE_NOTIFICATIONS},
      {"websitesLabelOn", IDS_WEBSITE_SETTINGS_CONTENT_SETTING_ENABLED},
      {"websitesLabelStorage", IDS_WEBSITE_SETTINGS_TYPE_STORAGE},
      {"websitesLabelBattery", IDS_WEBSITE_SETTINGS_TYPE_BATTERY},
      {"websitesCookiesDescription", IDS_WEBSITE_SETTINGS_COOKIES_DESCRIPTION},
      {"websitesLocationDescription",
       IDS_WEBSITE_SETTINGS_LOCATION_DESCRIPTION},
      {"websitesMediaStreamDescription",
       IDS_WEBSITE_SETTINGS_MEDIASTREAM_DESCRIPTION},
      {"websitesNotificationsDescription",
       IDS_WEBSITE_SETTINGS_NOTIFICATIONS_DESCRIPTION},
      {"websitesDownloadsDescription",
       IDS_WEBSITE_SETTINGS_DOWNLOAD_DESCRIPTION},
      {"websitesPluginsDescription", IDS_WEBSITE_SETTINGS_PLUGINS_DESCRIPTION},
      {"websitesPopupsDescription", IDS_WEBSITE_SETTINGS_POPUPS_DESCRIPTION},
      {"websitesJavascriptDescription",
       IDS_WEBSITE_SETTINGS_JAVASCRIPT_DESCRIPTION},
      {"websitesImagesDescription", IDS_WEBSITE_SETTINGS_IMAGES_DESCRIPTION},
      {"websitesButtonClear", IDS_WEBSITE_SETTINGS_STORAGE_CLEAR_BUTTON},
      {"websitesButtonStop", IDS_WEBSITE_SETTINGS_BATTERY_STOP_BUTTON},
      {"websitesAllowedListTitle", IDS_WEBSITE_SETTINGS_ALLOWED_LIST_TITLE},
      {"websitesBlockedListTitle", IDS_WEBSITE_SETTINGS_BLOCKED_LIST_TITLE},
      {"storageTabLabel", IDS_WEBSITE_SETTINGS_TYPE_STORAGE},
      {"batteryTabLabel", IDS_WEBSITE_SETTINGS_TYPE_BATTERY},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(
      localized_strings, "websiteSettingsPage", IDS_WEBSITE_SETTINGS_TITLE);
}

void WebsiteSettingsHandler::InitializeHandler() {
  Profile* profile = GetProfile();
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  observer_.Add(settings);

  power::OriginPowerMap* origin_power_map =
      power::OriginPowerMapFactory::GetForBrowserContext(profile);
  // OriginPowerMap may not be available in tests.
  if (origin_power_map) {
    subscription_ = origin_power_map->AddPowerConsumptionUpdatedCallback(
        base::Bind(&WebsiteSettingsHandler::Update, base::Unretained(this)));
  }
}

void WebsiteSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updateOrigins",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateOrigins,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "updateOriginsSearchResults",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateSearchResults,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "updateLocalStorage",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateLocalStorage,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "updateBatteryUsage",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateBatteryUsage,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getOriginInfo",
      base::Bind(&WebsiteSettingsHandler::HandleGetOriginInfo,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setOriginPermission",
      base::Bind(&WebsiteSettingsHandler::HandleSetOriginPermission,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "maybeShowEditPage",
      base::Bind(&WebsiteSettingsHandler::HandleMaybeShowEditPage,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "deleteLocalStorage",
      base::Bind(&WebsiteSettingsHandler::HandleDeleteLocalStorage,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "stopOrigin",
      base::Bind(&WebsiteSettingsHandler::HandleStopOrigin,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "updateDefaultSetting",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateDefaultSetting,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setDefaultContentSetting",
      base::Bind(&WebsiteSettingsHandler::HandleSetDefaultSetting,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setGlobalEnabled",
      base::Bind(&WebsiteSettingsHandler::HandleSetGlobalToggle,
                 base::Unretained(this)));
}

// content_settings::Observer implementation.
void WebsiteSettingsHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  Update();
}

void WebsiteSettingsHandler::OnContentSettingUsed(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  Update();
}

void WebsiteSettingsHandler::HandleUpdateOrigins(const base::ListValue* args) {
  std::string content_setting_name;
  bool rv = args->GetString(0, &content_setting_name);
  DCHECK(rv);

  ContentSettingsType content_type;
  rv = content_settings::GetTypeFromName(content_setting_name, &content_type);
  DCHECK(rv);
  DCHECK_NE(
      kValidTypes + kValidTypesLength,
      std::find(kValidTypes, kValidTypes + kValidTypesLength, content_type));

  last_setting_ = content_setting_name;
  UpdateOrigins();
}

void WebsiteSettingsHandler::HandleUpdateSearchResults(
    const base::ListValue* args) {
  bool rv = args->GetString(0, &last_filter_);
  DCHECK(rv);

  Update();
}

void WebsiteSettingsHandler::HandleUpdateLocalStorage(
    const base::ListValue* args) {
  if (!local_storage_.get()) {
    Profile* profile = GetProfile();
    local_storage_ = new BrowsingDataLocalStorageHelper(profile);
  }

  last_setting_ = kStorage;

  local_storage_->StartFetching(
      base::Bind(&WebsiteSettingsHandler::OnLocalStorageFetched,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebsiteSettingsHandler::HandleMaybeShowEditPage(
    const base::ListValue* args) {
  std::string site;
  bool rv = args->GetString(0, &site);
  DCHECK(rv);

  GURL last_site(site);
  if (!last_site.is_valid())
    return;

  last_site_ = last_site;
  base::StringValue site_value(site);
  web_ui()->CallJavascriptFunction("WebsiteSettingsEditor.showEditPage",
                                   site_value);
}

void WebsiteSettingsHandler::OnLocalStorageFetched(const std::list<
    BrowsingDataLocalStorageHelper::LocalStorageInfo>& storage) {
  local_storage_list_ = storage;
  Update();
  GetInfoForOrigin(last_site_, false);
}

void WebsiteSettingsHandler::Update() {
  DCHECK(!last_setting_.empty());
  if (last_setting_ == kStorage)
    UpdateLocalStorage();
  else if (last_setting_ == kBattery)
    UpdateBatteryUsage();
  else
    UpdateOrigins();
}

void WebsiteSettingsHandler::UpdateOrigins() {
  Profile* profile = GetProfile();
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();

  ContentSettingsForOneType all_settings;
  ContentSettingsType last_setting;
  content_settings::GetTypeFromName(last_setting_, &last_setting);

  if (last_setting == CONTENT_SETTINGS_TYPE_MEDIASTREAM)
    last_setting = CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC;

  settings->GetSettingsForOneType(last_setting, std::string(), &all_settings);

  base::DictionaryValue allowed_origins;
  base::DictionaryValue blocked_origins;
  for (ContentSettingsForOneType::const_iterator it = all_settings.begin();
       it != all_settings.end();
       ++it) {
    // Don't add default settings.
    if (it->primary_pattern == ContentSettingsPattern::Wildcard() &&
        it->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        it->source != kPreferencesSource) {
      continue;
    }

    GURL origin_url(it->primary_pattern.ToString());
    std::string origin = origin_url.spec();

    // Hide the port if it is using a standard URL scheme.
    if ((origin_url.SchemeIs(url::kHttpScheme) &&
         origin_url.IntPort() == kHttpPort) ||
        (origin_url.SchemeIs(url::kHttpsScheme) &&
         origin_url.IntPort() == kHttpsPort)) {
      url::Replacements<char> replacements;
      replacements.ClearPort();
      origin = origin_url.ReplaceComponents(replacements).spec();
    }

    // Mediastream isn't set unless both mic and camera are set to the same.
    if (last_setting == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
      ContentSetting cam_setting = settings->GetContentSettingWithoutOverride(
          origin_url,
          origin_url,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
          std::string());
      if (it->setting != cam_setting)
        continue;
    }

    if (origin.find(last_filter_) == base::string16::npos)
      continue;

    base::Time last_usage = settings->GetLastUsageByPattern(
        it->primary_pattern, it->secondary_pattern, last_setting);

    base::DictionaryValue* origin_entry = new base::DictionaryValue();
    origin_entry->SetDoubleWithoutPathExpansion("usage",
                                                last_usage.ToDoubleT());
    base::string16 usage_string;
    if (last_usage.ToDoubleT()) {
      usage_string = ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                            ui::TimeFormat::LENGTH_SHORT,
                                            base::Time::Now() - last_usage);
    }
    origin_entry->SetStringWithoutPathExpansion("usageString", usage_string);
    origin_entry->SetStringWithoutPathExpansion("readableName",
                                                GetReadableName(origin_url));

    if (it->setting == CONTENT_SETTING_BLOCK)
      blocked_origins.SetWithoutPathExpansion(origin, origin_entry);
    else
      allowed_origins.SetWithoutPathExpansion(origin, origin_entry);
  }

  bool is_globally_allowed = settings->GetContentSettingOverride(last_setting);
  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.populateOrigins",
                                   allowed_origins,
                                   blocked_origins,
                                   base::FundamentalValue(is_globally_allowed));
}

void WebsiteSettingsHandler::HandleGetOriginInfo(const base::ListValue* args) {
  std::string url;
  bool rv = args->GetString(0, &url);
  DCHECK(rv);
  GURL origin(url);

  if (!origin.is_valid())
    return;

  GetInfoForOrigin(origin, true);
}

void WebsiteSettingsHandler::HandleSetOriginPermission(
    const base::ListValue* args) {
  std::string setting_name;
  bool rv = args->GetString(0, &setting_name);
  DCHECK(rv);
  ContentSettingsType settings_type;
  rv = content_settings::GetTypeFromName(setting_name, &settings_type);
  DCHECK(rv);

  std::string value;
  rv = args->GetString(1, &value);
  DCHECK(rv);

  ContentSetting setting = content_settings::ContentSettingFromString(value);
  Profile* profile = GetProfile();
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  ContentSetting default_value =
      map->GetDefaultContentSetting(settings_type, NULL);

  // Users are not allowed to be the source of the "ask" setting. It is an
  // ephemeral setting which is removed once the question is asked.
  if (setting == CONTENT_SETTING_ASK && setting == default_value)
    setting = CONTENT_SETTING_DEFAULT;

  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  switch (settings_type) {
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      secondary_pattern = ContentSettingsPattern::Wildcard();
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      secondary_pattern = ContentSettingsPattern::Wildcard();
      map->SetContentSetting(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                             std::string(),
                             setting);
      map->SetContentSetting(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                             std::string(),
                             setting);
      return;
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
    case CONTENT_SETTINGS_TYPE_COOKIES:
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_IMAGES:
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
    case CONTENT_SETTINGS_TYPE_PLUGINS:
    case CONTENT_SETTINGS_TYPE_POPUPS:
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      secondary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      break;
    default:
      NOTREACHED() << "Content settings type not yet supported.";
      return;
  }

  content_settings::SettingInfo info;
  scoped_ptr<base::Value> v(map->GetWebsiteSettingWithoutOverride(
      last_site_, last_site_, settings_type, std::string(), &info));
  map->SetNarrowestWebsiteSetting(primary_pattern,
                                  secondary_pattern,
                                  settings_type,
                                  std::string(),
                                  setting,
                                  info);
}

void WebsiteSettingsHandler::HandleUpdateBatteryUsage(
    const base::ListValue* args) {
  last_setting_ = kBattery;
  UpdateBatteryUsage();
}

void WebsiteSettingsHandler::HandleDeleteLocalStorage(
    const base::ListValue* args) {
  DCHECK(!last_site_.is_empty());
  DeleteLocalStorage(last_site_);
}

void WebsiteSettingsHandler::HandleStopOrigin(const base::ListValue* args) {
  DCHECK(!last_site_.is_empty());
  StopOrigin(last_site_);
}

// TODO(dhnishi): Remove default settings duplication from the
//                WebsiteSettingsHandler and the ContentSettingsHandler.
void WebsiteSettingsHandler::HandleUpdateDefaultSetting(
    const base::ListValue* args) {
  ContentSettingsType last_setting;
  content_settings::GetTypeFromName(last_setting_, &last_setting);

  base::DictionaryValue filter_settings;
  std::string provider_id;
  filter_settings.SetString(
      "value", GetSettingDefaultFromModel(last_setting, &provider_id));
  filter_settings.SetString("managedBy", provider_id);

  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.updateDefault",
                                   filter_settings);
}

void WebsiteSettingsHandler::HandleSetDefaultSetting(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  std::string setting;
  if (!args->GetString(0, &setting)) {
    NOTREACHED();
    return;
  }
  ContentSetting new_default =
      content_settings::ContentSettingFromString(setting);

  ContentSettingsType last_setting;
  content_settings::GetTypeFromName(last_setting_, &last_setting);
  Profile* profile = GetProfile();

  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  map->SetDefaultContentSetting(last_setting, new_default);

  switch (last_setting) {
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMultipleAutomaticDLSettingChange"));
      break;
    case CONTENT_SETTINGS_TYPE_COOKIES:
      content::RecordAction(
          UserMetricsAction("Options_DefaultCookieSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      content::RecordAction(
          UserMetricsAction("Options_DefaultGeolocationSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_IMAGES:
      content::RecordAction(
          UserMetricsAction("Options_DefaultImagesSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      content::RecordAction(
          UserMetricsAction("Options_DefaultJavaScriptSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMediaStreamMicSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultNotificationsSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultPluginsSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultPopupsSettingChanged"));
      break;
    default:
      NOTREACHED();
      return;
  }
}

void WebsiteSettingsHandler::HandleSetGlobalToggle(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  bool is_enabled;
  bool rv = args->GetBoolean(0, &is_enabled);
  DCHECK(rv);

  ContentSettingsType last_setting;
  rv = content_settings::GetTypeFromName(last_setting_, &last_setting);
  DCHECK(rv);

  Profile* profile = GetProfile();
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  map->SetContentSettingOverride(last_setting, is_enabled);
}

void WebsiteSettingsHandler::GetInfoForOrigin(const GURL& site_url,
                                              bool show_page) {
  Profile* profile = GetProfile();
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();

  double storage = 0.0;
  for (LocalStorageList::const_iterator it = local_storage_list_.begin();
       it != local_storage_list_.end();
       it++) {
    if (it->origin_url == site_url) {
      storage = static_cast<double>(it->size);
      break;
    }
  }

  int battery = 0;
  battery = OriginPowerMapFactory::GetForBrowserContext(
      GetProfile())->GetPowerForOrigin(site_url);

  base::DictionaryValue* permissions = new base::DictionaryValue;
  for (size_t i = 0; i < arraysize(kValidTypes); ++i) {
    ContentSettingsType permission_type = kValidTypes[i];

    // Append the possible settings.
    base::ListValue* options = new base::ListValue;
    ContentSetting default_value =
        map->GetDefaultContentSetting(permission_type, NULL);
    if (default_value != CONTENT_SETTING_ALLOW &&
        default_value != CONTENT_SETTING_BLOCK) {
      options->AppendString(
          content_settings::ContentSettingToString(default_value));
    }
    options->AppendString(
        content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
    options->AppendString(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
    if (permission_type == CONTENT_SETTINGS_TYPE_COOKIES) {
      options->AppendString(content_settings::ContentSettingToString(
          CONTENT_SETTING_SESSION_ONLY));
    }

    ContentSetting permission;
    content_settings::SettingInfo info;
    if (permission_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
      scoped_ptr<base::Value> mic_value(map->GetWebsiteSettingWithoutOverride(
          site_url,
          site_url,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          std::string(),
          &info));
      ContentSetting mic_setting =
          content_settings::ValueToContentSetting(mic_value.get());
      ContentSetting cam_setting = map->GetContentSettingWithoutOverride(
          site_url,
          site_url,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
          std::string());

      if (mic_setting != cam_setting)
        permission = CONTENT_SETTING_ASK;
      else
        permission = mic_setting;
    } else {
      scoped_ptr<base::Value> v(map->GetWebsiteSettingWithoutOverride(
          site_url, site_url, permission_type, std::string(), &info));
      permission = content_settings::ValueToContentSetting(v.get());
    }

    base::DictionaryValue* permission_info = new base::DictionaryValue;
    permission_info->SetStringWithoutPathExpansion(
        "setting", content_settings::ContentSettingToString(permission));
    permission_info->SetWithoutPathExpansion("options", options);
    permission_info->SetBooleanWithoutPathExpansion(
        "editable", info.source == content_settings::SETTING_SOURCE_USER);
    permissions->SetWithoutPathExpansion(
        content_settings::GetTypeName(permission_type), permission_info);
  }

  base::Value* storage_used = new base::StringValue(l10n_util::GetStringFUTF16(
      IDS_WEBSITE_SETTINGS_STORAGE_USED, ui::FormatBytes(storage)));
  base::Value* battery_used =
      new base::StringValue(l10n_util::GetStringFUTF16Int(
          IDS_WEBSITE_SETTINGS_BATTERY_USED, battery));

  web_ui()->CallJavascriptFunction("WebsiteSettingsEditor.populateOrigin",
                                   *storage_used,
                                   *battery_used,
                                   *permissions,
                                   base::FundamentalValue(show_page));
}

void WebsiteSettingsHandler::UpdateLocalStorage() {
  base::DictionaryValue local_storage_map;
  for (LocalStorageList::const_iterator it = local_storage_list_.begin();
       it != local_storage_list_.end();
       it++) {
    std::string origin = it->origin_url.spec();

    if (origin.find(last_filter_) == base::string16::npos)
      continue;

    base::DictionaryValue* origin_entry = new base::DictionaryValue();
    origin_entry->SetWithoutPathExpansion(
        "usage", new base::FundamentalValue(static_cast<double>(it->size)));
    origin_entry->SetWithoutPathExpansion(
        "usageString", new base::StringValue(ui::FormatBytes(it->size)));
    origin_entry->SetStringWithoutPathExpansion(
        "readableName", GetReadableName(it->origin_url));
    local_storage_map.SetWithoutPathExpansion(origin, origin_entry);
  }
  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.populateOrigins",
                                   local_storage_map);
}

void WebsiteSettingsHandler::UpdateBatteryUsage() {
  base::DictionaryValue power_map;
  OriginPowerMap* origins =
      OriginPowerMapFactory::GetForBrowserContext(GetProfile());
  OriginPowerMap::PercentOriginMap percent_map = origins->GetPercentOriginMap();
  for (std::map<GURL, int>::iterator it = percent_map.begin();
       it != percent_map.end();
       ++it) {
    std::string origin = it->first.spec();

    if (origin.find(last_filter_) == base::string16::npos)
      continue;

    base::DictionaryValue* origin_entry = new base::DictionaryValue();
    origin_entry->SetInteger("usage", it->second);
    if (it->second == 0) {
      origin_entry->SetString(
          "usageString",
          l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_BATTERY_ZERO_PERCENT));
    } else {
      origin_entry->SetString(
          "usageString",
          l10n_util::GetStringFUTF16Int(IDS_WEBSITE_SETTINGS_BATTERY_PERCENT,
                                        it->second));
    }
    origin_entry->SetStringWithoutPathExpansion("readableName",
                                                GetReadableName(it->first));
    power_map.SetWithoutPathExpansion(origin, origin_entry);
  }
  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.populateOrigins",
                                   power_map);
}

std::string WebsiteSettingsHandler::GetSettingDefaultFromModel(
    ContentSettingsType type,
    std::string* provider_id) {
  Profile* profile = GetProfile();
  ContentSetting default_setting =
      profile->GetHostContentSettingsMap()->GetDefaultContentSetting(
          type, provider_id);

  return content_settings::ContentSettingToString(default_setting);
}

void WebsiteSettingsHandler::StopOrigin(const GURL& site_url) {
  Profile* profile = GetProfile();
  if (site_url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)
            ->enabled_extensions()
            .GetHostedAppByURL(site_url);
    if (extension) {
      extensions::AppWindowRegistry::Get(profile)
          ->CloseAllAppWindowsForApp(extension->id());
    }
  }

  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    TabStripModel* model = browser->tab_strip_model();
    for (int idx = 0; idx < model->count(); idx++) {
      content::WebContents* web_contents = model->GetWebContentsAt(idx);
      // Can't discard tabs that are already discarded or active.
      if (model->IsTabDiscarded(idx) || (model->active_index() == idx))
        continue;

      // Don't discard tabs that belong to other profiles or other origins.
      if (web_contents->GetLastCommittedURL().GetOrigin() != site_url ||
          profile !=
              Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
        continue;
      }
      model->DiscardWebContentsAt(idx);
    }
  }
}

void WebsiteSettingsHandler::DeleteLocalStorage(const GURL& site_url) {
  Profile* profile = GetProfile();
  content::DOMStorageContext* dom_storage_context_ =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetDOMStorageContext();
  dom_storage_context_->DeleteLocalStorage(site_url);

  // Load a new BrowsingDataLocalStorageHelper to update.
  local_storage_ = new BrowsingDataLocalStorageHelper(profile);

  local_storage_->StartFetching(
      base::Bind(&WebsiteSettingsHandler::OnLocalStorageFetched,
                 weak_ptr_factory_.GetWeakPtr()));
}

const std::string& WebsiteSettingsHandler::GetReadableName(
    const GURL& site_url) {
  if (site_url.SchemeIs(extensions::kExtensionScheme)) {
    Profile* profile = GetProfile();
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();

    const extensions::Extension* extension =
        extension_service->extensions()->GetExtensionOrAppByURL(site_url);
    // If extension is NULL, it was removed and we cannot look up its name.
    if (!extension)
      return site_url.spec();

    return extension->name();
  }
  return site_url.spec();
}

Profile* WebsiteSettingsHandler::GetProfile() {
  Profile* profile = Profile::FromWebUI(web_ui());
#if defined(OS_CHROMEOS)
  // Chrome OS special case: in Guest mode settings are opened in Incognito
  // mode, so we need original profile to actually modify settings.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetOriginalProfile();
#endif
  return profile;
}

}  // namespace options
