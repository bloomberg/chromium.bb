// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plugins/plugins_ui.h"

#include <stddef.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/plugins/plugins_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#endif

namespace {

content::WebUIDataSource* CreatePluginsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPluginsHost);

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
  source->AddLocalizedString("alwaysAllowed", IDS_PLUGINS_ALWAYS_ALLOWED);
  source->AddLocalizedString("noPlugins", IDS_PLUGINS_NO_PLUGINS);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("plugins.js", IDR_PLUGINS_JS);
  source->SetDefaultResource(IDR_PLUGINS_HTML);
#if defined(OS_CHROMEOS)
  chromeos::AddAccountUITweaksLocalizedValues(source, profile);
#endif
  source->AddResourcePath("chrome/browser/ui/webui/plugins/plugins.mojom",
                          IDR_PLUGINS_MOJO_JS);

  return source;
}

}  // namespace

PluginsUI::PluginsUI(content::WebUI* web_ui) : MojoWebUIController(web_ui) {
  // Set up the chrome://plugins/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreatePluginsUIHTMLSource(profile));
}

PluginsUI::~PluginsUI() {}

// static
base::RefCountedMemory* PluginsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_PLUGINS_FAVICON, scale_factor);
}

// static
void PluginsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kPluginsShowDetails, false);
  registry->RegisterDictionaryPref(
      prefs::kContentSettingsPluginWhitelist,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void PluginsUI::BindUIHandler(
    mojo::InterfaceRequest<mojom::PluginsPageHandler> request) {
  plugins_handler_.reset(new PluginsPageHandler(web_ui(), std::move(request)));
}
