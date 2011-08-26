// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plugins_ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
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
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

ChromeWebUIDataSource* CreatePluginsUIHTMLSource(bool enable_controls) {
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

  if (!enable_controls) {
    source->AddLocalizedString("pluginsDisabledHeader",
                               IDS_PLUGINS_DISABLED_HEADER);
    source->AddLocalizedString("pluginsDisabledText",
                               IDS_PLUGINS_DISABLED_TEXT);
  }

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
  // Loads the plugins on the FILE thread.
  static void LoadPluginsOnFileThread(
      std::vector<webkit::npapi::PluginGroup>* groups, Task* task);

  // Used in conjunction with ListWrapper to avoid any memory leaks.
  static void EnsurePluginGroupsDeleted(
      std::vector<webkit::npapi::PluginGroup>* groups);

  // Call this to start getting the plugins on the UI thread.
  void LoadPlugins();

  // Called on the UI thread when the plugin information is ready.
  void PluginsLoaded(const std::vector<webkit::npapi::PluginGroup>* groups);

  NotificationRegistrar registrar_;

  ScopedRunnableMethodFactory<PluginsDOMHandler> get_plugins_factory_;

  // This pref guards the value whether about:plugins is in the details mode or
  // not.
  BooleanPrefMember show_details_;

  DISALLOW_COPY_AND_ASSIGN(PluginsDOMHandler);
};

PluginsDOMHandler::PluginsDOMHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(get_plugins_factory_(this)) {
  registrar_.Add(this,
                 content::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
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

  // If a non-first-profile user tries to trigger these methods sneakily,
  // forbid it.
#if !defined(OS_CHROMEOS)
  if (!profile->GetOriginalProfile()->first_launched())
    return;
#endif

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
          webkit::npapi::PluginGroup::kAdobeReaderGroupName);
      string16 internalpdf =
          ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName);
      if (group_name == adobereader) {
        plugin_prefs->EnablePluginGroup(false, internalpdf);
      } else if (group_name == internalpdf) {
        plugin_prefs->EnablePluginGroup(false, adobereader);
      }
    }
  } else {
    FilePath::StringType file_path;
    if (!args->GetString(0, &file_path))
      return;

    plugin_prefs->EnablePlugin(enable, FilePath(file_path));
  }

  // TODO(viettrungluu): We might also want to ensure that the plugins
  // list is always written to prefs even when the user hasn't disabled a
  // plugin. <http://crbug.com/39101>
  plugin_prefs->UpdatePreferences(0);
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
  DCHECK_EQ(content::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED, type);
  LoadPlugins();
}

void PluginsDOMHandler::LoadPluginsOnFileThread(
    std::vector<webkit::npapi::PluginGroup>* groups,
    Task* task) {
  webkit::npapi::PluginList::Singleton()->GetPluginGroups(true, groups);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, task);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableFunction(&PluginsDOMHandler::EnsurePluginGroupsDeleted,
                          groups));
}

void PluginsDOMHandler::EnsurePluginGroupsDeleted(
    std::vector<webkit::npapi::PluginGroup>* groups) {
  delete groups;
}

void PluginsDOMHandler::LoadPlugins() {
  if (!get_plugins_factory_.empty())
    return;

  std::vector<webkit::npapi::PluginGroup>* groups =
      new std::vector<webkit::npapi::PluginGroup>;
  Task* task = get_plugins_factory_.NewRunnableMethod(
          &PluginsDOMHandler::PluginsLoaded, groups);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableFunction(
          &PluginsDOMHandler::LoadPluginsOnFileThread, groups, task));
}

void PluginsDOMHandler::PluginsLoaded(
    const std::vector<webkit::npapi::PluginGroup>* groups) {
  // Construct DictionaryValues to return to the UI
  ListValue* plugin_groups_data = new ListValue();
  for (size_t i = 0; i < groups->size(); ++i) {
    plugin_groups_data->Append((*groups)[i].GetDataForUI());
    // TODO(bauerb): Fetch plugin enabled state from PluginPrefs.
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
  bool enable_controls = true;
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
#if !defined(OS_CHROMEOS)
  enable_controls = profile->GetOriginalProfile()->first_launched();
#endif
  profile->GetChromeURLDataManager()->AddDataSource(
      CreatePluginsUIHTMLSource(enable_controls));
}


// static
RefCountedMemory* PluginsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_PLUGIN);
}

// static
void PluginsUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kPluginsPluginsList,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsEnabledInternalPDF,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsEnabledNaCl,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsShowDetails,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsShowSetReaderDefaultInfobar,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}
