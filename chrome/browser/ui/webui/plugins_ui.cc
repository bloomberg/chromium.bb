// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plugins_ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/plugin_service.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_group.h"

using webkit::npapi::PluginGroup;
using webkit::WebPluginInfo;

namespace {

ChromeWebUIDataSource* CreatePluginsUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIPluginsHost);

  source->AddLocalizedString("pluginsTitle", IDS_PLUGINS_TITLE);
  source->AddLocalizedString("pluginsDetailsModeLink",
                             IDS_PLUGINS_DETAILS_MODE_LINK);
  source->AddLocalizedString("pluginsNoneInstalled",
                             IDS_PLUGINS_NONE_INSTALLED);
  source->AddLocalizedString("pluginDisabled", IDS_PLUGINS_DISABLED_PLUGIN);
  source->AddLocalizedString("pluginDisabledByPolicy",
                             IDS_PLUGINS_DISABLED_BY_POLICY_PLUGIN);
  source->AddLocalizedString("pluginCannotBeEnabledDueToPolicy",
                             IDS_PLUGINS_CANNOT_ENABLE_DUE_TO_POLICY);
  source->AddLocalizedString("pluginEnabledByPolicy",
                             IDS_PLUGINS_ENABLED_BY_POLICY_PLUGIN);
  source->AddLocalizedString("pluginCannotBeDisabledDueToPolicy",
                             IDS_PLUGINS_CANNOT_DISABLE_DUE_TO_POLICY);
  source->AddLocalizedString("pluginDownload", IDS_PLUGINS_DOWNLOAD);
  source->AddLocalizedString("pluginName", IDS_PLUGINS_NAME);
  source->AddLocalizedString("pluginVersion", IDS_PLUGINS_VERSION);
  source->AddLocalizedString("pluginDescription", IDS_PLUGINS_DESCRIPTION);
  source->AddLocalizedString("pluginPath", IDS_PLUGINS_PATH);
  source->AddLocalizedString("pluginMimeTypes", IDS_PLUGINS_MIME_TYPES);
  source->AddLocalizedString("pluginMimeTypesMimeType",
                             IDS_PLUGINS_MIME_TYPES_MIME_TYPE);
  source->AddLocalizedString("pluginMimeTypesDescription",
                             IDS_PLUGINS_MIME_TYPES_DESCRIPTION);
  source->AddLocalizedString("pluginMimeTypesFileExtensions",
                             IDS_PLUGINS_MIME_TYPES_FILE_EXTENSIONS);
  source->AddLocalizedString("disable", IDS_PLUGINS_DISABLE);
  source->AddLocalizedString("enable", IDS_PLUGINS_ENABLE);
  source->AddLocalizedString("noPlugins", IDS_PLUGINS_NO_PLUGINS);

  source->set_json_path("strings.js");
  source->add_resource_path("plugins.js", IDR_PLUGINS_JS);
  source->set_default_resource(IDR_PLUGINS_HTML);
  return source;
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
                          public NotificationObserver {
 public:
  explicit PluginsDOMHandler();
  virtual ~PluginsDOMHandler() {}

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "requestPluginsData" message.
  void HandleRequestPluginsData(const ListValue* args);

  // Callback for the "enablePlugin" message.
  void HandleEnablePluginMessage(const ListValue* args);

  // Callback for the "saveShowDetailsToPrefs" message.
  void HandleSaveShowDetailsToPrefs(const ListValue* args);

  // Calback for the "getShowDetails" message.
  void HandleGetShowDetails(const ListValue* args);

  // NotificationObserver method overrides
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // Call this to start getting the plugins on the UI thread.
  void LoadPlugins();

  // Called on the UI thread when the plugin information is ready.
  void PluginsLoaded(const std::vector<PluginGroup>& groups);

  NotificationRegistrar registrar_;

  base::WeakPtrFactory<PluginsDOMHandler> weak_ptr_factory_;

  // This pref guards the value whether about:plugins is in the details mode or
  // not.
  BooleanPrefMember show_details_;

  DISALLOW_COPY_AND_ASSIGN(PluginsDOMHandler);
};

PluginsDOMHandler::PluginsDOMHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 NotificationService::AllSources());
}

WebUIMessageHandler* PluginsDOMHandler::Attach(WebUI* web_ui) {
  PrefService* prefs = Profile::FromWebUI(web_ui)->GetPrefs();

  show_details_.Init(prefs::kPluginsShowDetails, prefs, NULL);

  return WebUIMessageHandler::Attach(web_ui);
}

void PluginsDOMHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("requestPluginsData",
      NewCallback(this, &PluginsDOMHandler::HandleRequestPluginsData));
  web_ui_->RegisterMessageCallback("enablePlugin",
      NewCallback(this, &PluginsDOMHandler::HandleEnablePluginMessage));
  web_ui_->RegisterMessageCallback("saveShowDetailsToPrefs",
      NewCallback(this, &PluginsDOMHandler::HandleSaveShowDetailsToPrefs));
  web_ui_->RegisterMessageCallback("getShowDetails",
      NewCallback(this, &PluginsDOMHandler::HandleGetShowDetails));
}

void PluginsDOMHandler::HandleRequestPluginsData(const ListValue* args) {
  LoadPlugins();
}

void PluginsDOMHandler::HandleEnablePluginMessage(const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui_);

  // Be robust in accepting badness since plug-ins display HTML (hence
  // JavaScript).
  if (args->GetSize() != 3)
    return;

  std::string enable_str;
  std::string is_group_str;
  if (!args->GetString(1, &enable_str) || !args->GetString(2, &is_group_str))
    return;
  bool enable = enable_str == "true";

  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile);
  if (is_group_str == "true") {
    string16 group_name;
    if (!args->GetString(0, &group_name))
      return;

    plugin_prefs->EnablePluginGroup(enable, group_name);
    if (enable) {
      // See http://crbug.com/50105 for background.
      string16 adobereader = ASCIIToUTF16(
          PluginGroup::kAdobeReaderGroupName);
      string16 internalpdf =
          ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName);
      if (group_name == adobereader)
        plugin_prefs->EnablePluginGroup(false, internalpdf);
      else if (group_name == internalpdf)
        plugin_prefs->EnablePluginGroup(false, adobereader);
    }
  } else {
    FilePath::StringType file_path;
    if (!args->GetString(0, &file_path))
      return;
    bool result = plugin_prefs->EnablePlugin(enable, FilePath(file_path));
    DCHECK(result);
  }
}

void PluginsDOMHandler::HandleSaveShowDetailsToPrefs(const ListValue* args) {
  std::string details_mode;
  if (!args->GetString(0, &details_mode)) {
    NOTREACHED();
    return;
  }
  show_details_.SetValue(details_mode == "true");
}

void PluginsDOMHandler::HandleGetShowDetails(const ListValue* args) {
  base::FundamentalValue show_details(show_details_.GetValue());
  web_ui_->CallJavascriptFunction("loadShowDetailsFromPrefs", show_details);
}

void PluginsDOMHandler::Observe(int type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED, type);
  LoadPlugins();
}

void PluginsDOMHandler::LoadPlugins() {
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  PluginService::GetInstance()->GetPluginGroups(
      base::Bind(&PluginsDOMHandler::PluginsLoaded,
          weak_ptr_factory_.GetWeakPtr()));
}

void PluginsDOMHandler::PluginsLoaded(const std::vector<PluginGroup>& groups) {
  PluginPrefs* plugin_prefs =
      PluginPrefs::GetForProfile(Profile::FromWebUI(web_ui_));

  bool all_plugins_enabled_by_policy = true;
  bool all_plugins_disabled_by_policy = true;

  // Construct DictionaryValues to return to the UI
  ListValue* plugin_groups_data = new ListValue();
  for (size_t i = 0; i < groups.size(); ++i) {
    ListValue* plugin_files = new ListValue();
    const PluginGroup& group = groups[i];
    string16 group_name = group.GetGroupName();
    bool group_enabled = false;
    const WebPluginInfo* active_plugin = NULL;
    for (size_t j = 0; j < group.web_plugin_infos().size(); ++j) {
      const WebPluginInfo& group_plugin = group.web_plugin_infos()[j];

      DictionaryValue* plugin_file = new DictionaryValue();
      plugin_file->SetString("name", group_plugin.name);
      plugin_file->SetString("description", group_plugin.desc);
      plugin_file->SetString("path", group_plugin.path.value());
      plugin_file->SetString("version", group_plugin.version);

      ListValue* mime_types = new ListValue();
      const std::vector<webkit::WebPluginMimeType>& plugin_mime_types =
          group_plugin.mime_types;
      for (size_t k = 0; k < plugin_mime_types.size(); ++k) {
        DictionaryValue* mime_type = new DictionaryValue();
        mime_type->SetString("mimeType", plugin_mime_types[k].mime_type);
        mime_type->SetString("description", plugin_mime_types[k].description);

        ListValue* file_extensions = new ListValue();
        const std::vector<std::string>& mime_file_extensions =
            plugin_mime_types[k].file_extensions;
        for (size_t l = 0; l < mime_file_extensions.size(); ++l)
          file_extensions->Append(new StringValue(mime_file_extensions[l]));
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
      } else {
        all_plugins_enabled_by_policy = false;
        if (plugin_status == PluginPrefs::POLICY_DISABLED ||
          group_status == PluginPrefs::POLICY_DISABLED) {
          enabled_mode = "disabledByPolicy";
        } else {
          all_plugins_disabled_by_policy = false;
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
    DictionaryValue* group_data = new DictionaryValue();

    group_data->Set("plugin_files", plugin_files);
    group_data->SetString("name", group_name);
    group_data->SetString("description", active_plugin->desc);
    group_data->SetString("version", active_plugin->version);
    group_data->SetBoolean("critical", group.IsVulnerable(*active_plugin));
    group_data->SetString("update_url", group.GetUpdateURL());

    std::string enabled_mode;
    if (all_plugins_enabled_by_policy) {
      enabled_mode = "enabledByPolicy";
    } else if (all_plugins_disabled_by_policy) {
      enabled_mode = "disabledByPolicy";
    } else if (group_enabled) {
      enabled_mode = "enabledByUser";
    } else {
      enabled_mode = "disabledByUser";
    }
    group_data->SetString("enabledMode", enabled_mode);

    plugin_groups_data->Append(group_data);
  }
  DictionaryValue results;
  results.Set("plugins", plugin_groups_data);
  web_ui_->CallJavascriptFunction("returnPluginsData", results);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// PluginsUI
//
///////////////////////////////////////////////////////////////////////////////

PluginsUI::PluginsUI(TabContents* contents) : ChromeWebUI(contents) {
  AddMessageHandler((new PluginsDOMHandler())->Attach(this));

  // Set up the chrome://plugins/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      CreatePluginsUIHTMLSource());
}


// static
RefCountedMemory* PluginsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_PLUGIN);
}

// static
void PluginsUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPluginsShowDetails,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsShowSetReaderDefaultInfobar,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}
