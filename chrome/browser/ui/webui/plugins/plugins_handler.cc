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
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_constants.h"
#include "mojo/common/common_type_converters.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebPluginInfo;
// Holds grouped plugins. The key is the group identifier and
// the value is the list of plugins belonging to the group.
using PluginGroups =
    base::hash_map<std::string, std::vector<const content::WebPluginInfo*>>;

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

mojo::Array<MimeTypePtr> GeneratePluginMimeTypes(const WebPluginInfo& plugin) {
  mojo::Array<MimeTypePtr> mime_types(0u);
  for (const auto& plugin_mime_type : plugin.mime_types) {
    MimeTypePtr mime_type(MimeType::New());
    mime_type->description = mojo::String::From(plugin_mime_type.description);

    mime_type->mime_type = mojo::String::From(plugin_mime_type.mime_type);
    mime_type->file_extensions =
        mojo::Array<mojo::String>::From(plugin_mime_type.file_extensions);
    mime_types.push_back(std::move(mime_type));
  }

  return mime_types;
}

}  // namespace

PluginsHandler::PluginsHandler(
    content::WebUI* web_ui,
    mojo::InterfaceRequest<PluginsHandlerMojo> request)
    : web_ui_(web_ui),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  PrefService* prefs = profile->GetPrefs();
  show_details_.Init(prefs::kPluginsShowDetails, prefs);

  registrar_.Add(this, chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 content::Source<Profile>(profile));
}

PluginsHandler::~PluginsHandler() {}

void PluginsHandler::SetPluginEnabled(const mojo::String& plugin_path,
                                      bool enable) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();
  plugin_prefs->EnablePlugin(
      enable, base::FilePath(plugin_path.To<base::FilePath::StringType>()),
      base::Bind(&AssertPluginEnabled));
}

void PluginsHandler::SetPluginGroupEnabled(const mojo::String& group_name,
                                           bool enable) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();
  base::string16 group_name_as_string16 = group_name.To<base::string16>();
  plugin_prefs->EnablePluginGroup(enable, group_name_as_string16);
  if (!enable) {
    return;
  }

  // See http://crbug.com/50105 for background.
  base::string16 adobereader =
      base::ASCIIToUTF16(PluginMetadata::kAdobeReaderGroupName);
  base::string16 internalpdf =
      base::ASCIIToUTF16(ChromeContentClient::kPDFPluginName);
  if (group_name_as_string16 == adobereader)
    plugin_prefs->EnablePluginGroup(false, internalpdf);
  else if (group_name_as_string16 == internalpdf)
    plugin_prefs->EnablePluginGroup(false, adobereader);
}

void PluginsHandler::GetShowDetails(const GetShowDetailsCallback& callback) {
  callback.Run(show_details_.GetValue());
}

void PluginsHandler::SaveShowDetailsToPrefs(bool details_mode) {
  show_details_.SetValue(details_mode);
}

void PluginsHandler::SetPluginAlwaysAllowed(const mojo::String& plugin,
                                            bool allowed) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  HostContentSettingsMapFactory::GetForProfile(profile)->SetContentSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS, plugin.get(),
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_DEFAULT);

  // Keep track of the whitelist separately, so that we can distinguish plugins
  // whitelisted by the user from automatically whitelisted ones.
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kContentSettingsPluginWhitelist);
  update->SetBoolean(plugin, allowed);
}

void PluginsHandler::GetPluginsData(const GetPluginsDataCallback& callback) {
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&PluginsHandler::RespondWithPluginsData,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PluginsHandler::SetClientPage(PluginsPageMojoPtr page) {
  page_ = std::move(page);
}

void PluginsHandler::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED, type);

  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  content::PluginService::GetInstance()->GetPlugins(base::Bind(
      &PluginsHandler::NotifyWithPluginsData, weak_ptr_factory_.GetWeakPtr()));
}

void PluginsHandler::RespondWithPluginsData(
    const GetPluginsDataCallback& callback,
    const std::vector<WebPluginInfo>& plugins) {
  callback.Run(GeneratePluginsData(plugins));
}

void PluginsHandler::NotifyWithPluginsData(
    const std::vector<WebPluginInfo>& plugins) {
  if (page_)
    page_->OnPluginsUpdated(GeneratePluginsData(plugins));
}

mojo::Array<PluginDataPtr> PluginsHandler::GeneratePluginsData(
    const std::vector<WebPluginInfo>& plugins) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();

  PluginFinder* plugin_finder = PluginFinder::GetInstance();
  // Group plugins by identifier. This is done to be able to display
  // the plugins in UI in a grouped fashion.
  PluginGroups groups;
  for (size_t i = 0; i < plugins.size(); ++i) {
    scoped_ptr<PluginMetadata> plugin(
        plugin_finder->GetPluginMetadata(plugins[i]));
    groups[plugin->identifier()].push_back(&plugins[i]);
  }

  mojo::Array<PluginDataPtr> plugins_data(0u);

  for (PluginGroups::const_iterator it = groups.begin(); it != groups.end();
       ++it) {
    PluginDataPtr plugin_data(PluginData::New());
    const std::vector<const WebPluginInfo*>& group_plugins = it->second;

    scoped_ptr<PluginMetadata> plugin_metadata(
        plugin_finder->GetPluginMetadata(*group_plugins[0]));
    std::string group_identifier = plugin_metadata->identifier();
    plugin_data->id = mojo::String::From(group_identifier);

    const WebPluginInfo* active_plugin = nullptr;
    bool group_enabled = false;

    mojo::Array<PluginFilePtr> plugin_files(0u);
    for (const auto& group_plugin : group_plugins) {
      bool plugin_enabled = plugin_prefs->IsPluginEnabled(*group_plugin);

      plugin_files.push_back(GeneratePluginFile(
          *group_plugin, plugin_metadata->name(), plugin_enabled));

      // Update |active_plugin| and |group_enabled|.
      if (!active_plugin || (plugin_enabled && !group_enabled))
        active_plugin = group_plugin;
      group_enabled = plugin_enabled || group_enabled;
    }

    plugin_data->enabled_mode = mojo::String::From(
        GetPluginGroupEnabledMode(plugin_files, group_enabled));

    plugin_data->always_allowed = false;
    if (group_enabled) {
      const base::DictionaryValue* whitelist =
          profile->GetPrefs()->GetDictionary(
              prefs::kContentSettingsPluginWhitelist);
      whitelist->GetBoolean(group_identifier, &plugin_data->always_allowed);
    }

    plugin_data->critical = false;
    plugin_data->update_url = "";
#if defined(ENABLE_PLUGIN_INSTALLATION)
    bool out_of_date = plugin_metadata->GetSecurityStatus(*active_plugin) ==
                       PluginMetadata::SECURITY_STATUS_OUT_OF_DATE;
    plugin_data->critical = out_of_date;
    plugin_data->update_url = plugin_metadata->plugin_url().spec();
#endif

    plugin_data->description = mojo::String::From(active_plugin->desc);
    plugin_data->name = base::UTF16ToUTF8(plugin_metadata->name());
    plugin_data->plugin_files = std::move(plugin_files);
    plugin_data->version = mojo::String::From(active_plugin->version);
    plugins_data.push_back(std::move(plugin_data));
  }

  return plugins_data;
}

PluginFilePtr PluginsHandler::GeneratePluginFile(
    const WebPluginInfo& plugin,
    const base::string16& group_name,
    bool plugin_enabled) const {
  PluginFilePtr plugin_file(PluginFile::New());
  plugin_file->description = mojo::String::From(GetPluginDescription(plugin));
  plugin_file->enabled_mode = mojo::String::From(
      GetPluginEnabledMode(plugin.name, group_name, plugin_enabled));
  plugin_file->name = mojo::String::From(plugin.name);
  plugin_file->path = mojo::String::From(plugin.path.value());
  plugin_file->type = mojo::String::From(PluginTypeToString(plugin.type));
  plugin_file->version = mojo::String::From(plugin.version);
  plugin_file->mime_types = GeneratePluginMimeTypes(plugin);

  return plugin_file;
}

std::string PluginsHandler::GetPluginEnabledMode(
    const base::string16& plugin_name,
    const base::string16& group_name,
    bool plugin_enabled) const {
  Profile* profile = Profile::FromWebUI(web_ui_);
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
    const mojo::Array<PluginFilePtr>& plugin_files,
    bool group_enabled) const {
  bool plugins_enabled_by_policy = true;
  bool plugins_disabled_by_policy = true;
  bool plugins_managed_by_policy = true;

  for (size_t i = 0; i < plugin_files.size(); i++) {
    std::string plugin_enabled_mode = plugin_files[i]->enabled_mode;

    plugins_enabled_by_policy =
        plugins_enabled_by_policy && plugin_enabled_mode == "enabledByPolicy";
    plugins_disabled_by_policy =
        plugins_disabled_by_policy && plugin_enabled_mode == "disabledByPolicy";
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
