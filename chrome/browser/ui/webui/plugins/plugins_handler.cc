// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plugins/plugins_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_constants.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebPluginInfo;
// Holds grouped plugins. The key is the group identifier and
// the value is the list of plugins belonging to the group.
using PluginGroups = base::hash_map<std::string,
      std::vector<const content::WebPluginInfo*>>;

namespace {

// Callback function to process result of EnablePlugin method.
void AssertPluginEnabled(bool did_enable) {
  DCHECK(did_enable);
}

base::string16 PluginTypeToString(int type) {
  // The type is stored as an |int|, but doing the switch on the right
  // enumeration type gives us better build-time error checking (if someone adds
  // a new type).
  switch (static_cast<WebPluginInfo::PluginType>(type)) {
    case WebPluginInfo::PLUGIN_TYPE_NPAPI:
      return l10n_util::GetStringUTF16(IDS_PLUGINS_NPAPI);
    case WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS:
      return l10n_util::GetStringUTF16(IDS_PLUGINS_PPAPI_IN_PROCESS);
    case WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS:
      return l10n_util::GetStringUTF16(IDS_PLUGINS_PPAPI_OUT_OF_PROCESS);
    case WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN:
      return l10n_util::GetStringUTF16(IDS_PLUGINS_BROWSER_PLUGIN);
  }
  NOTREACHED();
  return base::string16();
}

base::string16 GetPluginDescription(const WebPluginInfo& plugin) {
  // If this plugin is Pepper Flash, and the plugin path is the same as the
  // path for the Pepper Flash System plugin, then mark this plugin
  // description as the system plugin to help the user disambiguate the
  // two plugins.
  base::string16 desc = plugin.desc;
  if (plugin.is_pepper_plugin() &&
      plugin.name == base::ASCIIToUTF16(content::kFlashPluginName)) {
    base::FilePath system_flash_path;
    PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
                     &system_flash_path);
    if (base::FilePath::CompareEqualIgnoreCase(plugin.path.value(),
                                               system_flash_path.value())) {
#if defined(GOOGLE_CHROME_BUILD)
      // Existing documentation for debugging Flash describe this plugin as
      // "Debug" so preserve this nomenclature here.
      desc += base::ASCIIToUTF16(" Debug");
#else
      // On Chromium, we can name it what it really is; the system plugin.
      desc += base::ASCIIToUTF16(" System");
#endif
    }
  }
  return desc;
}

scoped_ptr<base::ListValue> GetPluginMimeTypes(const WebPluginInfo& plugin) {
  scoped_ptr<base::ListValue> mime_types(new base::ListValue());
  for (const auto& plugin_mime_type: plugin.mime_types) {
    base::DictionaryValue* mime_type = new base::DictionaryValue();
    mime_type->SetString("mimeType", plugin_mime_type.mime_type);
    mime_type->SetString("description", plugin_mime_type.description);

    base::ListValue* file_extensions = new base::ListValue();
    for (const auto& mime_file_extension : plugin_mime_type.file_extensions) {
      file_extensions->Append(new base::StringValue(mime_file_extension));
    }
    mime_type->Set("fileExtensions", file_extensions);
    mime_types->Append(mime_type);
  }
  return mime_types;
}

}  // namespace


PluginsHandler::PluginsHandler() : weak_ptr_factory_(this) {
}

PluginsHandler::~PluginsHandler() {
}

void PluginsHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());

  PrefService* prefs = profile->GetPrefs();
  show_details_.Init(prefs::kPluginsShowDetails, prefs);

  registrar_.Add(this,
                 chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 content::Source<Profile>(profile));

  web_ui()->RegisterMessageCallback("requestPluginsData",
      base::Bind(&PluginsHandler::HandleRequestPluginsData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enablePlugin",
      base::Bind(&PluginsHandler::HandleEnablePluginMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setPluginAlwaysAllowed",
      base::Bind(&PluginsHandler::HandleSetPluginAlwaysAllowed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("saveShowDetailsToPrefs",
      base::Bind(&PluginsHandler::HandleSaveShowDetailsToPrefs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getShowDetails",
      base::Bind(&PluginsHandler::HandleGetShowDetails,
                 base::Unretained(this)));
}

void PluginsHandler::HandleRequestPluginsData(const base::ListValue* args) {
  LoadPlugins();
}

void PluginsHandler::HandleEnablePluginMessage(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  // Be robust in accepting badness since plugins display HTML (hence
  // JavaScript).
  if (args->GetSize() != 3) {
    NOTREACHED();
    return;
  }

  std::string enable_str;
  std::string is_group_str;
  if (!args->GetString(1, &enable_str) || !args->GetString(2, &is_group_str)) {
    NOTREACHED();
    return;
  }
  bool enable = enable_str == "true";

  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();
  if (is_group_str == "true") {
    base::string16 group_name;
    if (!args->GetString(0, &group_name)) {
      NOTREACHED();
      return;
    }

    plugin_prefs->EnablePluginGroup(enable, group_name);
    if (enable) {
      // See http://crbug.com/50105 for background.
      base::string16 adobereader = base::ASCIIToUTF16(
          PluginMetadata::kAdobeReaderGroupName);
      base::string16 internalpdf =
          base::ASCIIToUTF16(ChromeContentClient::kPDFPluginName);
      if (group_name == adobereader)
        plugin_prefs->EnablePluginGroup(false, internalpdf);
      else if (group_name == internalpdf)
        plugin_prefs->EnablePluginGroup(false, adobereader);
    }
  } else {
    base::FilePath::StringType file_path;
    if (!args->GetString(0, &file_path)) {
      NOTREACHED();
      return;
    }

    plugin_prefs->EnablePlugin(enable, base::FilePath(file_path),
                               base::Bind(&AssertPluginEnabled));
  }
}

void PluginsHandler::HandleSaveShowDetailsToPrefs(
    const base::ListValue* args) {
  std::string details_mode;
  if (!args->GetString(0, &details_mode)) {
    NOTREACHED();
    return;
  }
  show_details_.SetValue(details_mode == "true");
}

void PluginsHandler::HandleGetShowDetails(const base::ListValue* args) {
  base::FundamentalValue show_details(show_details_.GetValue());
  web_ui()->CallJavascriptFunction("loadShowDetailsFromPrefs", show_details);
}

void PluginsHandler::HandleSetPluginAlwaysAllowed(
    const base::ListValue* args) {
  // Be robust in the input parameters, but crash in a Debug build.
  if (args->GetSize() != 2) {
    NOTREACHED();
    return;
  }

  std::string plugin;
  bool allowed = false;
  if (!args->GetString(0, &plugin) || !args->GetBoolean(1, &allowed)) {
    NOTREACHED();
    return;
  }
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMapFactory::GetForProfile(profile)->SetContentSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      plugin,
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_DEFAULT);

  // Keep track of the whitelist separately, so that we can distinguish plugins
  // whitelisted by the user from automatically whitelisted ones.
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kContentSettingsPluginWhitelist);
  update->SetBoolean(plugin, allowed);
}

void PluginsHandler::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED, type);
  LoadPlugins();
}

void PluginsHandler::LoadPlugins() {
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&PluginsHandler::PluginsLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PluginsHandler::PluginsLoaded(
    const std::vector<WebPluginInfo>& plugins) {
  Profile* profile = Profile::FromWebUI(web_ui());
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();

  ContentSettingsPattern wildcard = ContentSettingsPattern::Wildcard();

  PluginFinder* plugin_finder = PluginFinder::GetInstance();
  // Group plugins by identifier. This is done to be able to display
  // the plugins in UI in a grouped fashion.
  PluginGroups groups;
  for (size_t i = 0; i < plugins.size(); ++i) {
    scoped_ptr<PluginMetadata> plugin(
        plugin_finder->GetPluginMetadata(plugins[i]));
    groups[plugin->identifier()].push_back(&plugins[i]);
  }

  // Construct DictionaryValues to return to UI.
  base::ListValue* plugin_groups_data = new base::ListValue();
  for (PluginGroups::const_iterator it = groups.begin();
      it != groups.end(); ++it) {
    const std::vector<const WebPluginInfo*>& group_plugins = it->second;
    base::ListValue* plugin_files = new base::ListValue();
    scoped_ptr<PluginMetadata> plugin_metadata(
        plugin_finder->GetPluginMetadata(*group_plugins[0]));
    base::string16 group_name = plugin_metadata->name();
    std::string group_identifier = plugin_metadata->identifier();
    bool group_enabled = false;
    const WebPluginInfo* active_plugin = NULL;
    for (size_t j = 0; j < group_plugins.size(); ++j) {
      const WebPluginInfo& group_plugin = *group_plugins[j];

      base::DictionaryValue* plugin_file = new base::DictionaryValue();
      plugin_file->SetString("name", group_plugin.name);
      plugin_file->SetString("description", GetPluginDescription(group_plugin));
      plugin_file->SetString("path", group_plugin.path.value());
      plugin_file->SetString("version", group_plugin.version);
      plugin_file->SetString("type", PluginTypeToString(group_plugin.type));
      plugin_file->Set("mimeTypes", GetPluginMimeTypes(group_plugin));

      bool plugin_enabled = plugin_prefs->IsPluginEnabled(group_plugin);
      plugin_file->SetString(
          "enabledMode",
          GetPluginEnabledMode(group_plugin.name, group_name, plugin_enabled));
      plugin_files->Append(plugin_file);

      if (!active_plugin || (plugin_enabled && !group_enabled))
        active_plugin = &group_plugin;
      group_enabled = plugin_enabled || group_enabled;
    }

    base::DictionaryValue* group_data = new base::DictionaryValue();
    group_data->Set("plugin_files", plugin_files);
    group_data->SetString("name", group_name);
    group_data->SetString("id", group_identifier);
    group_data->SetString("description", active_plugin->desc);
    group_data->SetString("version", active_plugin->version);

#if defined(ENABLE_PLUGIN_INSTALLATION)
    bool out_of_date = plugin_metadata->GetSecurityStatus(*active_plugin) ==
        PluginMetadata::SECURITY_STATUS_OUT_OF_DATE;
    group_data->SetBoolean("critical", out_of_date);
    group_data->SetString("update_url", plugin_metadata->plugin_url().spec());
#endif

    group_data->SetString(
        "enabledMode", GetPluginGroupEnabledMode(*plugin_files, group_enabled));

    bool always_allowed = false;
    if (group_enabled) {
      const base::DictionaryValue* whitelist =
          profile->GetPrefs()->GetDictionary(
              prefs::kContentSettingsPluginWhitelist);
      whitelist->GetBoolean(group_identifier, &always_allowed);
    }
    group_data->SetBoolean("alwaysAllowed", always_allowed);

    plugin_groups_data->Append(group_data);
  }
  base::DictionaryValue results;
  results.Set("plugins", plugin_groups_data);
  web_ui()->CallJavascriptFunction("returnPluginsData", results);
}

std::string PluginsHandler::GetPluginEnabledMode(
    const base::string16& plugin_name,
    const base::string16& group_name,
    bool plugin_enabled) const {
  Profile* profile = Profile::FromWebUI(web_ui());
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();
  PluginPrefs::PolicyStatus plugin_status =
      plugin_prefs->PolicyStatusForPlugin(plugin_name);
  PluginPrefs::PolicyStatus group_status =
      plugin_prefs->PolicyStatusForPlugin(group_name);

  if (plugin_status == PluginPrefs::POLICY_ENABLED ||
      group_status == PluginPrefs::POLICY_ENABLED) {
    return "enabledByPolicy";
  }
  if (plugin_status == PluginPrefs::POLICY_DISABLED ||
      group_status == PluginPrefs::POLICY_DISABLED) {
    return "disabledByPolicy";
  }
  return plugin_enabled ? "enabledByUser" : "disabledByUser";
}

std::string PluginsHandler::GetPluginGroupEnabledMode(
    const base::ListValue& plugin_files, bool group_enabled) const {
  bool plugins_enabled_by_policy = true;
  bool plugins_disabled_by_policy = true;
  bool plugins_managed_by_policy = true;

  for (base::ListValue::const_iterator it = plugin_files.begin();
       it != plugin_files.end(); ++it) {
    base::DictionaryValue* plugin_dict;
    CHECK((*it)->GetAsDictionary(&plugin_dict));
    std::string plugin_enabled_mode;
    CHECK(plugin_dict->GetString("enabledMode", &plugin_enabled_mode));

    plugins_enabled_by_policy = plugins_enabled_by_policy &&
        plugin_enabled_mode == "enabledByPolicy";
    plugins_disabled_by_policy = plugins_disabled_by_policy &&
        plugin_enabled_mode == "disabledByPolicy";
    plugins_managed_by_policy = plugins_managed_by_policy &&
        (plugin_enabled_mode == "enabledByPolicy" ||
         plugin_enabled_mode == "disabledByPolicy");
  }

  if (plugins_enabled_by_policy)
    return "enabledByPolicy";
  if (plugins_disabled_by_policy)
    return "disabledByPolicy";
  if (plugins_managed_by_policy)
    return "managedByPolicy";
  return group_enabled ? "enabledByUser" : "disabledByUser";
}
