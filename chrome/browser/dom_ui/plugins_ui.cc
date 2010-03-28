// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/plugins_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
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

 private:
  // Creates a dictionary containing all the information about the given plugin;
  // this is put into the list to "return" for the "requestPluginsData" message.
  DictionaryValue* CreatePluginDetailValue(const WebPluginInfo& plugin);

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
}

void PluginsDOMHandler::HandleRequestPluginsData(const Value* value) {
  DictionaryValue* results = new DictionaryValue();

  // Add plugins to the results structure.
  ListValue* plugins_list = new ListValue();

  std::vector<WebPluginInfo> plugins;
  NPAPI::PluginList::Singleton()->GetPlugins(false, &plugins);

  for (std::vector<WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    plugins_list->Append(CreatePluginDetailValue(*it));
  }
  results->Set(L"plugins", plugins_list);

  dom_ui_->CallJavascriptFunction(L"returnPluginsData", *results);
}

void PluginsDOMHandler::HandleEnablePluginMessage(const Value* value) {
  CHECK(value->IsType(Value::TYPE_LIST));
  const ListValue* list = static_cast<const ListValue*>(value);
  CHECK(list->GetSize() == 2);
  FilePath::StringType plugin_path;
  std::string enable_str;
  CHECK(list->GetString(0, &plugin_path));
  CHECK(list->GetString(1, &enable_str));
  if (enable_str == "true")
    NPAPI::PluginList::Singleton()->EnablePlugin(FilePath(plugin_path));
  else
    NPAPI::PluginList::Singleton()->DisablePlugin(FilePath(plugin_path));

  // TODO(viettrungluu): It's morally wrong to do this here (it should be done
  // by the plugins service), and we might also want to ensure that the plugins
  // list is always written to prefs even when the user hasn't disabled a
  // plugin. This will require refactoring the plugin list and service.
  // <http://crbug.com/39101>
  UpdatePreferences();
}

DictionaryValue* PluginsDOMHandler::CreatePluginDetailValue(
    const WebPluginInfo& plugin) {
  DictionaryValue* plugin_data = new DictionaryValue();
  plugin_data->SetString(L"path", plugin.path.value());
  plugin_data->SetString(L"name", plugin.name);
  plugin_data->SetString(L"version", plugin.version);
  plugin_data->SetString(L"description", plugin.desc);
  plugin_data->SetBoolean(L"enabled", plugin.enabled);

  ListValue* mime_types = new ListValue();
  for (std::vector<WebPluginMimeType>::const_iterator type_it =
           plugin.mime_types.begin();
       type_it != plugin.mime_types.end();
       ++type_it) {
    DictionaryValue* mime_type = new DictionaryValue();
    mime_type->SetString(L"mimeType", type_it->mime_type);
    mime_type->SetString(L"description", type_it->description);

    ListValue* file_extensions = new ListValue();
    for (std::vector<std::string>::const_iterator ext_it =
             type_it->file_extensions.begin();
         ext_it != type_it->file_extensions.end();
         ++ext_it) {
      file_extensions->Append(new StringValue(*ext_it));
    }
    mime_type->Set(L"fileExtensions", file_extensions);

    mime_types->Append(mime_type);
  }
  plugin_data->Set(L"mimeTypes", mime_types);

  return plugin_data;
}

DictionaryValue* PluginsDOMHandler::CreatePluginSummaryValue(
    const WebPluginInfo& plugin) {
  DictionaryValue* plugin_data = new DictionaryValue();
  plugin_data->SetString(L"path", plugin.path.value());
  plugin_data->SetString(L"name", plugin.name);
  plugin_data->SetString(L"version", plugin.version);
  plugin_data->SetBoolean(L"enabled", plugin.enabled);
  return plugin_data;
}

void PluginsDOMHandler::UpdatePreferences() {
  ListValue* plugins_list = dom_ui_->GetProfile()->GetPrefs()->GetMutableList(
      prefs::kPluginsPluginsList);
  plugins_list->Clear();

  std::vector<WebPluginInfo> plugins;
  NPAPI::PluginList::Singleton()->GetPlugins(false, &plugins);

  for (std::vector<WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    plugins_list->Append(CreatePluginSummaryValue(*it));
  }
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
  prefs->RegisterListPref(prefs::kPluginsPluginsList);
}
