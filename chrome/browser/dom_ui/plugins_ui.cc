// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/plugins_ui.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/plugin_updater.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// PluginsHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class PluginsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  PluginsUIHTMLSource()
      : DataSource(chrome::kChromeUIPluginsHost, MessageLoop::current()) {}

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~PluginsUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(PluginsUIHTMLSource);
};

void PluginsUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_off_the_record,
                                           int request_id) {
  // Strings used in the JsTemplate file.
  DictionaryValue localized_strings;
  localized_strings.SetString(L"pluginsTitle",
      l10n_util::GetString(IDS_PLUGINS_TITLE));
  localized_strings.SetString(L"pluginsDetailsModeLink",
      l10n_util::GetString(IDS_PLUGINS_DETAILS_MODE_LINK));
  localized_strings.SetString(L"pluginsNoneInstalled",
      l10n_util::GetString(IDS_PLUGINS_NONE_INSTALLED));
  localized_strings.SetString(L"pluginDisabled",
      l10n_util::GetString(IDS_PLUGINS_DISABLED_PLUGIN));
  localized_strings.SetString(L"pluginDisabledByPolicy",
      l10n_util::GetString(IDS_PLUGINS_DISABLED_BY_POLICY_PLUGIN));
  localized_strings.SetString(L"pluginCannotBeEnabledDueToPolicy",
      l10n_util::GetString(IDS_PLUGINS_CANNOT_ENABLE_DUE_TO_POLICY));
  localized_strings.SetString(L"pluginDownload",
      l10n_util::GetString(IDS_PLUGINS_DOWNLOAD));
  localized_strings.SetString(L"pluginName",
      l10n_util::GetString(IDS_PLUGINS_NAME));
  localized_strings.SetString(L"pluginPriority",
      l10n_util::GetString(IDS_PLUGINS_PRIORITY));
  localized_strings.SetString(L"pluginVersion",
      l10n_util::GetString(IDS_PLUGINS_VERSION));
  localized_strings.SetString(L"pluginDescription",
      l10n_util::GetString(IDS_PLUGINS_DESCRIPTION));
  localized_strings.SetString(L"pluginPath",
      l10n_util::GetString(IDS_PLUGINS_PATH));
  localized_strings.SetString(L"pluginMimeTypes",
      l10n_util::GetString(IDS_PLUGINS_MIME_TYPES));
  localized_strings.SetString(L"pluginMimeTypesMimeType",
      l10n_util::GetString(IDS_PLUGINS_MIME_TYPES_MIME_TYPE));
  localized_strings.SetString(L"pluginMimeTypesDescription",
      l10n_util::GetString(IDS_PLUGINS_MIME_TYPES_DESCRIPTION));
  localized_strings.SetString(L"pluginMimeTypesFileExtensions",
      l10n_util::GetString(IDS_PLUGINS_MIME_TYPES_FILE_EXTENSIONS));
  localized_strings.SetString(L"disable",
      l10n_util::GetString(IDS_PLUGINS_DISABLE));
  localized_strings.SetString(L"enable",
      l10n_util::GetString(IDS_PLUGINS_ENABLE));

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece plugins_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_PLUGINS_HTML));
  std::string full_html(plugins_html.data(), plugins_html.size());
  jstemplate_builder::AppendJsonHtml(&localized_strings, &full_html);
  jstemplate_builder::AppendI18nTemplateSourceHtml(&full_html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
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
class PluginsDOMHandler : public DOMMessageHandler {
 public:
  PluginsDOMHandler() {}
  virtual ~PluginsDOMHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for the "requestPluginsData" message.
  void HandleRequestPluginsData(const Value* value);

  // Callback for the "enablePlugin" message.
  void HandleEnablePluginMessage(const Value* value);

  // Callback for the "showTermsOfService" message. This really just opens a new
  // window with about:terms. Flash can't link directly to about:terms due to
  // the security model.
  void HandleShowTermsOfServiceMessage(const Value* value);

 private:
  // Creates a dictionary containing all the information about the given plugin;
  // this is put into the list to "return" for the "requestPluginsData" message.
  DictionaryValue* CreatePluginDetailValue(
      const WebPluginInfo& plugin,
      const std::set<string16>& plugin_blacklist_set);

  // Creates a dictionary containing the important parts of the information
  // about the given plugin; this is put into a list and saved in prefs.
  DictionaryValue* CreatePluginSummaryValue(const WebPluginInfo& plugin);

  // Update the user preferences to reflect the current (user-selected) state of
  // plugins.
  void UpdatePreferences();

  DISALLOW_COPY_AND_ASSIGN(PluginsDOMHandler);
};

void PluginsDOMHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("requestPluginsData",
      NewCallback(this, &PluginsDOMHandler::HandleRequestPluginsData));
  dom_ui_->RegisterMessageCallback("enablePlugin",
      NewCallback(this, &PluginsDOMHandler::HandleEnablePluginMessage));
  dom_ui_->RegisterMessageCallback("showTermsOfService",
      NewCallback(this, &PluginsDOMHandler::HandleShowTermsOfServiceMessage));
}

void PluginsDOMHandler::HandleRequestPluginsData(const Value* value) {
  DictionaryValue* results = new DictionaryValue();
  results->Set(L"plugins", plugin_updater::GetPluginGroupsData());

  dom_ui_->CallJavascriptFunction(L"returnPluginsData", *results);
}

void PluginsDOMHandler::HandleEnablePluginMessage(const Value* value) {
  // Be robust in accepting badness since plug-ins display HTML (hence
  // JavaScript).
  if (!value->IsType(Value::TYPE_LIST))
    return;

  const ListValue* list = static_cast<const ListValue*>(value);
  if (list->GetSize() != 3)
    return;

  std::string enable_str;
  std::string is_group_str;
  if (!list->GetString(1, &enable_str) || !list->GetString(2, &is_group_str))
    return;

  if (is_group_str == "true") {
    std::wstring group_name;
    if (!list->GetString(0, &group_name))
      return;

    plugin_updater::EnablePluginGroup(enable_str == "true",
                                      WideToUTF16(group_name));
  } else {
    FilePath::StringType file_path;
    if (!list->GetString(0, &file_path))
      return;

    plugin_updater::EnablePluginFile(enable_str == "true", file_path);
  }

  // TODO(viettrungluu): We might also want to ensure that the plugins
  // list is always written to prefs even when the user hasn't disabled a
  // plugin. <http://crbug.com/39101>
  plugin_updater::UpdatePreferences(dom_ui_->GetProfile());
}

void PluginsDOMHandler::HandleShowTermsOfServiceMessage(const Value* value) {
  // Show it in a new browser window....
  Browser* browser = Browser::Create(dom_ui_->GetProfile());
  browser->OpenURL(GURL(chrome::kAboutTermsURL),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// PluginsUI
//
///////////////////////////////////////////////////////////////////////////////

PluginsUI::PluginsUI(TabContents* contents) : DOMUI(contents) {
  AddMessageHandler((new PluginsDOMHandler())->Attach(this));

  PluginsUIHTMLSource* html_source = new PluginsUIHTMLSource();

  // Set up the chrome://plugins/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}


// static
RefCountedMemory* PluginsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_PLUGIN);
}

// static
void PluginsUI::RegisterUserPrefs(PrefService* prefs) {
  FilePath internal_dir;
  PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir);
  prefs->RegisterFilePathPref(prefs::kPluginsLastInternalDirectory,
                              internal_dir);

  prefs->RegisterListPref(prefs::kPluginsPluginsBlacklist);
  prefs->RegisterListPref(prefs::kPluginsPluginsList);
  prefs->RegisterBooleanPref(prefs::kPluginsEnabledInternalPDF, false);
}
