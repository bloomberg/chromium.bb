// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/uber/uber_ui.h"

#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "grit/browser_resources.h"

using content::NavigationController;
using content::NavigationEntry;
using content::RenderViewHost;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateUberHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUberHost);

  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");
  source->AddResourcePath("uber.js", IDR_UBER_JS);
  source->AddResourcePath("uber_utils.js", IDR_UBER_UTILS_JS);
  source->SetDefaultResource(IDR_UBER_HTML);
  source->OverrideContentSecurityPolicyFrameSrc("frame-src chrome:;");

  // Hack alert: continue showing "Loading..." until a real title is set.
  source->AddLocalizedString("pageTitle", IDS_TAB_LOADING_TITLE);

  source->AddString("extensionsFrameURL", chrome::kChromeUIExtensionsFrameURL);
  source->AddString("extensionsHost", chrome::kChromeUIExtensionsHost);
  source->AddString("helpFrameURL", chrome::kChromeUIHelpFrameURL);
  source->AddString("helpHost", chrome::kChromeUIHelpHost);
  source->AddString("historyFrameURL", chrome::kChromeUIHistoryFrameURL);
  source->AddString("historyHost", chrome::kChromeUIHistoryHost);
  source->AddString("settingsFrameURL", chrome::kChromeUISettingsFrameURL);
  source->AddString("settingsHost", chrome::kChromeUISettingsHost);

  return source;
}

// Determines whether the user has an active extension of the given type.
bool HasExtensionType(Profile* profile, const std::string& extension_type) {
  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator iter = extension_set.begin();
       iter != extension_set.end(); ++iter) {
    const extensions::URLOverrides::URLOverrideMap& map =
        extensions::URLOverrides::GetChromeURLOverrides(iter->get());
    if (ContainsKey(map, extension_type))
      return true;
  }

  return false;
}

content::WebUIDataSource* CreateUberFrameHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUberFrameHost);

  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");
  source->AddResourcePath("uber_frame.js", IDR_UBER_FRAME_JS);
  source->SetDefaultResource(IDR_UBER_FRAME_HTML);

  // TODO(jhawkins): Attempt to get rid of IDS_SHORT_PRODUCT_OS_NAME.
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("shortProductName", IDS_SHORT_PRODUCT_OS_NAME);
#else
  source->AddLocalizedString("shortProductName", IDS_SHORT_PRODUCT_NAME);
#endif  // defined(OS_CHROMEOS)

  // Group settings and help separately if settings in a window is enabled.
  std::string settings_group("settings_group");
  std::string other_group(
      ::switches::SettingsWindowEnabled() ? "other_group" : "settings_group");
  source->AddString("extensionsHost", chrome::kChromeUIExtensionsHost);
  source->AddLocalizedString("extensionsDisplayName",
                             IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);
  source->AddString("extensionsGroup", other_group);
  source->AddString("helpHost", chrome::kChromeUIHelpHost);
  source->AddLocalizedString("helpDisplayName", IDS_ABOUT_TITLE);
  source->AddString("helpGroup", settings_group);
  source->AddString("historyHost", chrome::kChromeUIHistoryHost);
  source->AddLocalizedString("historyDisplayName", IDS_HISTORY_TITLE);
  source->AddString("historyGroup", other_group);
  source->AddString("settingsHost", chrome::kChromeUISettingsHost);
  source->AddLocalizedString("settingsDisplayName", IDS_SETTINGS_TITLE);
  source->AddString("settingsGroup", settings_group);
  bool overridesHistory =
      HasExtensionType(profile, chrome::kChromeUIHistoryHost);
  source->AddString("overridesHistory", overridesHistory ? "yes" : "no");
  source->DisableDenyXFrameOptions();
  source->OverrideContentSecurityPolicyFrameSrc("frame-src chrome:;");

  return source;
}

}  // namespace

UberUI::UberUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateUberHTMLSource());

  RegisterSubpage(chrome::kChromeUIExtensionsFrameURL,
                  chrome::kChromeUIExtensionsHost);
  RegisterSubpage(chrome::kChromeUIHelpFrameURL,
                  chrome::kChromeUIHelpHost);
  RegisterSubpage(chrome::kChromeUIHistoryFrameURL,
                  chrome::kChromeUIHistoryHost);
  RegisterSubpage(chrome::kChromeUISettingsFrameURL,
                  chrome::kChromeUISettingsHost);
  RegisterSubpage(chrome::kChromeUIUberFrameURL,
                  chrome::kChromeUIUberHost);
}

UberUI::~UberUI() {
  STLDeleteValues(&sub_uis_);
}

void UberUI::RegisterSubpage(const std::string& page_url,
                             const std::string& page_host) {
  GURL page_gurl(page_url);
  content::WebUI* webui = web_ui()->GetWebContents()->CreateWebUI(page_gurl);

  webui->OverrideJavaScriptFrame(page_host);
  sub_uis_[page_url] = webui;
}

content::WebUI* UberUI::GetSubpage(const std::string& page_url) {
  if (!ContainsKey(sub_uis_, page_url))
    return NULL;
  return sub_uis_[page_url];
}

void UberUI::RenderViewCreated(RenderViewHost* render_view_host) {
  for (SubpageMap::iterator iter = sub_uis_.begin(); iter != sub_uis_.end();
       ++iter) {
    iter->second->GetController()->RenderViewCreated(render_view_host);
  }
}

void UberUI::RenderViewReused(RenderViewHost* render_view_host) {
  for (SubpageMap::iterator iter = sub_uis_.begin(); iter != sub_uis_.end();
       ++iter) {
    iter->second->GetController()->RenderViewReused(render_view_host);
  }
}

bool UberUI::OverrideHandleWebUIMessage(const GURL& source_url,
                                        const std::string& message,
                                        const base::ListValue& args) {
  // Find the appropriate subpage and forward the message.
  SubpageMap::iterator subpage = sub_uis_.find(source_url.GetOrigin().spec());
  if (subpage == sub_uis_.end()) {
    // The message was sent from the uber page itself.
    DCHECK_EQ(std::string(chrome::kChromeUIUberHost), source_url.host());
    return false;
  }

  // The message was sent from a subpage.
  // TODO(jam) fix this to use interface
  // return subpage->second->GetController()->OverrideHandleWebUIMessage(
  //     source_url, message, args);
  subpage->second->ProcessWebUIMessage(source_url, message, args);
  return true;
}

// UberFrameUI

UberFrameUI::UberFrameUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateUberFrameHTMLSource(profile));

  // Register as an observer for when extensions are loaded and unloaded.
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<Profile>(profile));
}

UberFrameUI::~UberFrameUI() {
}

void UberFrameUI::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    // We listen for notifications that indicate an extension has been loaded
    // (i.e., has been installed and/or enabled) or unloaded (i.e., has been
    // uninstalled and/or disabled). If one of these events has occurred, then
    // we must update the behavior of the History navigation element so that
    // it opens the history extension if one is installed and enabled or
    // opens the default history page if one is uninstalled or disabled.
    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED:
    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      Profile* profile = Profile::FromWebUI(web_ui());
      bool overrides_history =
          HasExtensionType(profile, chrome::kChromeUIHistoryHost);
      web_ui()->CallJavascriptFunction(
          "uber_frame.setNavigationOverride",
          base::StringValue(chrome::kChromeUIHistoryHost),
          base::StringValue(overrides_history ? "yes" : "no"));
      break;
    }
    default:
      NOTREACHED();
  }
}
