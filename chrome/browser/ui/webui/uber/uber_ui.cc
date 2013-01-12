// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/uber/uber_ui.h"

#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

using content::NavigationController;
using content::NavigationEntry;
using content::RenderViewHost;
using content::WebContents;

namespace {

ChromeWebUIDataSource* CreateUberHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIUberHost);

  source->set_use_json_js_format_v2();
  source->set_json_path("strings.js");
  source->add_resource_path("uber.js", IDR_UBER_JS);
  source->add_resource_path("uber_utils.js", IDR_UBER_UTILS_JS);
  source->set_default_resource(IDR_UBER_HTML);

  // Hack alert: continue showing "Loading..." until a real title is set.
  source->AddLocalizedString("pageTitle", IDS_TAB_LOADING_TITLE);

  source->AddString("extensionsFrameURL",
                    ASCIIToUTF16(chrome::kChromeUIExtensionsFrameURL));
  source->AddString("extensionsHost",
                    ASCIIToUTF16(chrome::kChromeUIExtensionsHost));
  source->AddString("helpFrameURL",
                    ASCIIToUTF16(chrome::kChromeUIHelpFrameURL));
  source->AddString("helpHost",
                    ASCIIToUTF16(chrome::kChromeUIHelpHost));
  source->AddString("historyFrameURL",
                    ASCIIToUTF16(chrome::kChromeUIHistoryFrameURL));
  source->AddString("historyHost",
                    ASCIIToUTF16(chrome::kChromeUIHistoryHost));
  source->AddString("settingsFrameURL",
                    ASCIIToUTF16(chrome::kChromeUISettingsFrameURL));
  source->AddString("settingsHost",
                    ASCIIToUTF16(chrome::kChromeUISettingsHost));

  return source;
}

// Determines whether the user has an active extension of the given type.
bool HasExtensionType(Profile* profile, const char* extensionType) {
  const ExtensionSet* extensionSet =
      profile->GetExtensionService()->extensions();

  for (ExtensionSet::const_iterator iter = extensionSet->begin();
       iter != extensionSet->end(); ++iter) {
    extensions::URLOverrides::URLOverrideMap map =
        extensions::URLOverrides::GetChromeURLOverrides(*iter);
    extensions::URLOverrides::URLOverrideMap::const_iterator result =
        map.find(std::string(extensionType));

    if (result != map.end())
      return true;
  }

  return false;
}

ChromeWebUIDataSource* CreateUberFrameHTMLSource(Profile* profile) {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIUberFrameHost);

  source->set_use_json_js_format_v2();
  source->set_json_path("strings.js");
  source->add_resource_path("uber_frame.js", IDR_UBER_FRAME_JS);
  source->set_default_resource(IDR_UBER_FRAME_HTML);

  // TODO(jhawkins): Attempt to get rid of IDS_SHORT_PRODUCT_OS_NAME.
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("shortProductName", IDS_SHORT_PRODUCT_OS_NAME);
#else
  source->AddLocalizedString("shortProductName", IDS_SHORT_PRODUCT_NAME);
#endif  // defined(OS_CHROMEOS)

  source->AddString("extensionsHost",
                    ASCIIToUTF16(chrome::kChromeUIExtensionsHost));
  source->AddLocalizedString("extensionsDisplayName",
                             IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);
  source->AddString("helpHost",
                    ASCIIToUTF16(chrome::kChromeUIHelpHost));
  source->AddLocalizedString("helpDisplayName", IDS_HELP_TITLE);
  source->AddString("historyHost",
                    ASCIIToUTF16(chrome::kChromeUIHistoryHost));
  source->AddLocalizedString("historyDisplayName", IDS_HISTORY_TITLE);
  source->AddString("settingsHost",
                    ASCIIToUTF16(chrome::kChromeUISettingsHost));
  source->AddLocalizedString("settingsDisplayName", IDS_SETTINGS_TITLE);
  bool overridesHistory = HasExtensionType(profile,
      chrome::kChromeUIHistoryHost);
  source->AddString("overridesHistory",
                    ASCIIToUTF16(overridesHistory ? "yes" : "no"));

  return source;
}

}  // namespace

UberUI::UberUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateUberHTMLSource());

  RegisterSubpage(chrome::kChromeUIExtensionsFrameURL);
  RegisterSubpage(chrome::kChromeUIHelpFrameURL);
  RegisterSubpage(chrome::kChromeUIHistoryFrameURL);
  RegisterSubpage(chrome::kChromeUISettingsFrameURL);
  RegisterSubpage(chrome::kChromeUIUberFrameURL);
}

UberUI::~UberUI() {
  STLDeleteValues(&sub_uis_);
}

void UberUI::RegisterSubpage(const std::string& page_url) {
  content::WebUI* webui =
      web_ui()->GetWebContents()->CreateWebUI(GURL(page_url));

  webui->SetFrameXPath("//iframe[starts-with(@src,'" + page_url + "')]");
  sub_uis_[page_url] = webui;
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
                                        const ListValue& args) {
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
  ChromeURLDataManager::AddDataSource(profile,
      CreateUberFrameHTMLSource(profile));

  // Register as an observer for when extensions are loaded and unloaded.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(profile));
}

UberFrameUI::~UberFrameUI() {
}

void UberFrameUI::Observe(int type, const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    // We listen for notifications that indicate an extension has been loaded
    // (i.e., has been installed and/or enabled) or unloaded (i.e., has been
    // uninstalled and/or disabled). If one of these events has occurred, then
    // we must update the behavior of the History navigation element so that
    // it opens the history extension if one is installed and enabled or
    // opens the default history page if one is uninstalled or disabled.
    case chrome::NOTIFICATION_EXTENSION_LOADED:
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      Profile* profile = Profile::FromWebUI(web_ui());
      bool overridesHistory = HasExtensionType(profile,
          chrome::kChromeUIHistoryHost);
      scoped_ptr<Value> controlsValue(
          Value::CreateStringValue(chrome::kChromeUIHistoryHost));
      scoped_ptr<Value> overrideValue(
          Value::CreateStringValue(overridesHistory ? "yes" : "no"));
      web_ui()->CallJavascriptFunction(
          "uber_frame.setNavigationOverride", *controlsValue, *overrideValue);
      break;
    }
    default:
      NOTREACHED();
  }
}
