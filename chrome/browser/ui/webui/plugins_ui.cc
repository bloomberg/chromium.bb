// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plugins_ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#endif

using content::PluginService;
using content::WebContents;
using content::WebPluginInfo;
using content::WebUIMessageHandler;

namespace {

// Callback function to process result of EnablePlugin method.
void AssertPluginEnabled(bool did_enable) {
  DCHECK(did_enable);
}

content::WebUIDataSource* CreatePluginsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPluginsHost);
  source->SetUseJsonJSFormatV2();

  source->AddLocalizedString("pluginsTitle", IDS_PLUGINS_TITLE);
  source->AddLocalizedString("pluginsDetailsModeLink",
                             IDS_PLUGINS_DETAILS_MODE_LINK);
  source->AddLocalizedString("pluginsNoneInstalled",
                             IDS_PLUGINS_NONE_INSTALLED);
  source->AddLocalizedString("pluginDisabled", IDS_PLUGINS_DISABLED_PLUGIN);
  source->AddLocalizedString("pluginDisabledByPolicy",
                             IDS_PLUGINS_DISABLED_BY_POLICY_PLUGIN);
  source->AddLocalizedString("pluginEnabledByPolicy",
                             IDS_PLUGINS_ENABLED_BY_POLICY_PLUGIN);
  source->AddLocalizedString("pluginGroupManagedByPolicy",
                             IDS_PLUGINS_GROUP_MANAGED_BY_POLICY);
  source->AddLocalizedString("pluginDownload", IDS_PLUGINS_DOWNLOAD);
  source->AddLocalizedString("pluginName", IDS_PLUGINS_NAME);
  source->AddLocalizedString("pluginVersion", IDS_PLUGINS_VERSION);
  source->AddLocalizedString("pluginDescription", IDS_PLUGINS_DESCRIPTION);
  source->AddLocalizedString("pluginPath", IDS_PLUGINS_PATH);
  source->AddLocalizedString("pluginType", IDS_PLUGINS_TYPE);
  source->AddLocalizedString("pluginMimeTypes", IDS_PLUGINS_MIME_TYPES);
  source->AddLocalizedString("pluginMimeTypesMimeType",
                             IDS_PLUGINS_MIME_TYPES_MIME_TYPE);
  source->AddLocalizedString("pluginMimeTypesDescription",
                             IDS_PLUGINS_MIME_TYPES_DESCRIPTION);
  source->AddLocalizedString("pluginMimeTypesFileExtensions",
                             IDS_PLUGINS_MIME_TYPES_FILE_EXTENSIONS);
  source->AddLocalizedString("disable", IDS_PLUGINS_DISABLE);
  source->AddLocalizedString("enable", IDS_PLUGINS_ENABLE);
  source->AddLocalizedString("alwaysAllowed", IDS_PLUGINS_ALWAYS_ALLOWED);
  source->AddLocalizedString("noPlugins", IDS_PLUGINS_NO_PLUGINS);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("plugins.js", IDR_PLUGINS_JS);
  source->SetDefaultResource(IDR_PLUGINS_HTML);
#if defined(OS_CHROMEOS)
  chromeos::AddAccountUITweaksLocalizedValues(source, profile);
#endif
  return source;
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
    case WebPluginInfo::PLUGIN_TYPE_PEPPER_UNSANDBOXED:
      return l10n_util::GetStringUTF16(IDS_PLUGINS_PPAPI_UNSANDBOXED);
    case WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN:
      return l10n_util::GetStringUTF16(IDS_PLUGINS_BROWSER_PLUGIN);
  }
  NOTREACHED();
  return base::string16();
}

////////////////////////////////////////////////////////////////////////////////
//
// PluginsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://plugins/ page.
// TODO(viettrungluu): Make plugin list updates notify, and then observe
// changes; maybe replumb plugin list through plugin service?
// <http://crbug.com/39101>
class PluginsDOMHandler : public WebUIMessageHandler,
                          public content::NotificationObserver {
 public:
  explicit PluginsDOMHandler();
  virtual ~PluginsDOMHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "requestPluginsData" message.
  void HandleRequestPluginsData(const base::ListValue* args);

  // Callback for the "enablePlugin" message.
  void HandleEnablePluginMessage(const base::ListValue* args);

  // Callback for the "saveShowDetailsToPrefs" message.
  void HandleSaveShowDetailsToPrefs(const base::ListValue* args);

  // Calback for the "getShowDetails" message.
  void HandleGetShowDetails(const base::ListValue* args);

  // Callback for the "setPluginAlwaysAllowed" message.
  void HandleSetPluginAlwaysAllowed(const base::ListValue* args);

  // content::NotificationObserver method overrides
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void LoadPlugins();

  // Called on the UI thread when the plugin information is ready.
  void PluginsLoaded(const std::vector<WebPluginInfo>& plugins);

  content::NotificationRegistrar registrar_;

  // Holds grouped plug-ins. The key is the group identifier and
  // the value is the list of plug-ins belonging to the group.
  typedef base::hash_map<std::string, std::vector<const WebPluginInfo*> >
      PluginGroups;

  // This pref guards the value whether about:plugins is in the details mode or
  // not.
  BooleanPrefMember show_details_;

  base::WeakPtrFactory<PluginsDOMHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginsDOMHandler);
};

PluginsDOMHandler::PluginsDOMHandler()
    : weak_ptr_factory_(this) {
}

void PluginsDOMHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());

  PrefService* prefs = profile->GetPrefs();
  show_details_.Init(prefs::kPluginsShowDetails, prefs);

  registrar_.Add(this,
                 chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 content::Source<Profile>(profile));

  web_ui()->RegisterMessageCallback("requestPluginsData",
      base::Bind(&PluginsDOMHandler::HandleRequestPluginsData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enablePlugin",
      base::Bind(&PluginsDOMHandler::HandleEnablePluginMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setPluginAlwaysAllowed",
      base::Bind(&PluginsDOMHandler::HandleSetPluginAlwaysAllowed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("saveShowDetailsToPrefs",
      base::Bind(&PluginsDOMHandler::HandleSaveShowDetailsToPrefs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getShowDetails",
      base::Bind(&PluginsDOMHandler::HandleGetShowDetails,
                 base::Unretained(this)));
}

void PluginsDOMHandler::HandleRequestPluginsData(const base::ListValue* args) {
  LoadPlugins();
}

void PluginsDOMHandler::HandleEnablePluginMessage(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  // Be robust in accepting badness since plug-ins display HTML (hence
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

void PluginsDOMHandler::HandleSaveShowDetailsToPrefs(
    const base::ListValue* args) {
  std::string details_mode;
  if (!args->GetString(0, &details_mode)) {
    NOTREACHED();
    return;
  }
  show_details_.SetValue(details_mode == "true");
}

void PluginsDOMHandler::HandleGetShowDetails(const base::ListValue* args) {
  base::FundamentalValue show_details(show_details_.GetValue());
  web_ui()->CallJavascriptFunction("loadShowDetailsFromPrefs", show_details);
}

void PluginsDOMHandler::HandleSetPluginAlwaysAllowed(
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
  profile->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      plugin,
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_DEFAULT);

  // Keep track of the whitelist separately, so that we can distinguish plug-ins
  // whitelisted by the user from automatically whitelisted ones.
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kContentSettingsPluginWhitelist);
  update->SetBoolean(plugin, allowed);
}

void PluginsDOMHandler::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED, type);
  LoadPlugins();
}

void PluginsDOMHandler::LoadPlugins() {
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  PluginService::GetInstance()->GetPlugins(
      base::Bind(&PluginsDOMHandler::PluginsLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PluginsDOMHandler::PluginsLoaded(
    const std::vector<WebPluginInfo>& plugins) {
  Profile* profile = Profile::FromWebUI(web_ui());
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();

  ContentSettingsPattern wildcard = ContentSettingsPattern::Wildcard();

  PluginFinder* plugin_finder = PluginFinder::GetInstance();
  // Group plug-ins by identifier. This is done to be able to display
  // the plug-ins in UI in a grouped fashion.
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
    bool all_plugins_enabled_by_policy = true;
    bool all_plugins_disabled_by_policy = true;
    bool all_plugins_managed_by_policy = true;
    const WebPluginInfo* active_plugin = NULL;
    for (size_t j = 0; j < group_plugins.size(); ++j) {
      const WebPluginInfo& group_plugin = *group_plugins[j];

      base::DictionaryValue* plugin_file = new base::DictionaryValue();
      plugin_file->SetString("name", group_plugin.name);

      // If this plugin is Pepper Flash, and the plugin path is the same as the
      // path for the Pepper Flash Debugger plugin, then mark this plugin
      // description as the debugger plugin to help the user disambiguate the
      // two plugins.
      base::string16 desc = group_plugin.desc;
      if (group_plugin.is_pepper_plugin() &&
          group_plugin.name == base::ASCIIToUTF16(content::kFlashPluginName)) {
        base::FilePath debug_path;
        PathService::Get(chrome::DIR_PEPPER_FLASH_DEBUGGER_PLUGIN, &debug_path);
        if (group_plugin.path.DirName() == debug_path)
          desc += base::ASCIIToUTF16(" Debug");
      }
      plugin_file->SetString("description", desc);

      plugin_file->SetString("path", group_plugin.path.value());
      plugin_file->SetString("version", group_plugin.version);
      plugin_file->SetString("type", PluginTypeToString(group_plugin.type));

      base::ListValue* mime_types = new base::ListValue();
      const std::vector<content::WebPluginMimeType>& plugin_mime_types =
          group_plugin.mime_types;
      for (size_t k = 0; k < plugin_mime_types.size(); ++k) {
        base::DictionaryValue* mime_type = new base::DictionaryValue();
        mime_type->SetString("mimeType", plugin_mime_types[k].mime_type);
        mime_type->SetString("description", plugin_mime_types[k].description);

        base::ListValue* file_extensions = new base::ListValue();
        const std::vector<std::string>& mime_file_extensions =
            plugin_mime_types[k].file_extensions;
        for (size_t l = 0; l < mime_file_extensions.size(); ++l) {
          file_extensions->Append(
              new base::StringValue(mime_file_extensions[l]));
        }
        mime_type->Set("fileExtensions", file_extensions);

        mime_types->Append(mime_type);
      }
      plugin_file->Set("mimeTypes", mime_types);

      bool plugin_enabled = plugin_prefs->IsPluginEnabled(group_plugin);

      if (!active_plugin || (plugin_enabled && !group_enabled))
        active_plugin = &group_plugin;
      group_enabled = plugin_enabled || group_enabled;

      std::string enabled_mode;
      PluginPrefs::PolicyStatus plugin_status =
          plugin_prefs->PolicyStatusForPlugin(group_plugin.name);
      PluginPrefs::PolicyStatus group_status =
          plugin_prefs->PolicyStatusForPlugin(group_name);
      if (plugin_status == PluginPrefs::POLICY_ENABLED ||
          group_status == PluginPrefs::POLICY_ENABLED) {
        enabled_mode = "enabledByPolicy";
        all_plugins_disabled_by_policy = false;
      } else {
        all_plugins_enabled_by_policy = false;
        if (plugin_status == PluginPrefs::POLICY_DISABLED ||
            group_status == PluginPrefs::POLICY_DISABLED) {
          enabled_mode = "disabledByPolicy";
        } else {
          all_plugins_disabled_by_policy = false;
          all_plugins_managed_by_policy = false;
          if (plugin_enabled) {
            enabled_mode = "enabledByUser";
          } else {
            enabled_mode = "disabledByUser";
          }
        }
      }
      plugin_file->SetString("enabledMode", enabled_mode);

      plugin_files->Append(plugin_file);
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

    std::string enabled_mode;
    if (all_plugins_enabled_by_policy) {
      enabled_mode = "enabledByPolicy";
    } else if (all_plugins_disabled_by_policy) {
      enabled_mode = "disabledByPolicy";
    } else if (all_plugins_managed_by_policy) {
      enabled_mode = "managedByPolicy";
    } else if (group_enabled) {
      enabled_mode = "enabledByUser";
    } else {
      enabled_mode = "disabledByUser";
    }
    group_data->SetString("enabledMode", enabled_mode);

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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// PluginsUI
//
///////////////////////////////////////////////////////////////////////////////

PluginsUI::PluginsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new PluginsDOMHandler());

  // Set up the chrome://plugins/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreatePluginsUIHTMLSource(profile));
}

// static
base::RefCountedMemory* PluginsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_PLUGINS_FAVICON, scale_factor);
}

// static
void PluginsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPluginsShowDetails,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kContentSettingsPluginWhitelist,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
