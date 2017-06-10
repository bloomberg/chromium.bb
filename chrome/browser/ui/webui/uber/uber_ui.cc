// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/uber/uber_ui.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/browser_side_navigation_policy.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/options/options_ui.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::RenderFrameHost;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateUberHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUberHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("uber.js", IDR_UBER_JS);
  source->AddResourcePath("uber_utils.js", IDR_UBER_UTILS_JS);
  source->SetDefaultResource(IDR_UBER_HTML);
  source->OverrideContentSecurityPolicyChildSrc("child-src chrome:;");

  // Hack alert: continue showing "Loading..." until a real title is set.
  source->AddLocalizedString("pageTitle", IDS_TAB_LOADING_TITLE);

  source->AddString("extensionsFrameURL", chrome::kChromeUIExtensionsFrameURL);
  source->AddString("extensionsHost", chrome::kChromeUIExtensionsHost);
  source->AddString("helpFrameURL", chrome::kChromeUIHelpFrameURL);
  source->AddString("helpHost", chrome::kChromeUIHelpHost);
  source->AddString("settingsFrameURL", chrome::kChromeUISettingsFrameURL);
  source->AddString("settingsHost", chrome::kChromeUISettingsHost);

  return source;
}

content::WebUIDataSource* CreateUberFrameHTMLSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUberFrameHost);
  Profile* profile = Profile::FromBrowserContext(browser_context);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("uber_frame.js", IDR_UBER_FRAME_JS);
  source->SetDefaultResource(IDR_UBER_FRAME_HTML);

  // TODO(jhawkins): Attempt to get rid of IDS_SHORT_PRODUCT_OS_NAME.
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("shortProductName", IDS_SHORT_PRODUCT_OS_NAME);
#else
  source->AddLocalizedString("shortProductName", IDS_SHORT_PRODUCT_NAME);
#endif  // defined(OS_CHROMEOS)

  source->AddBoolean("hideExtensions",
      base::FeatureList::IsEnabled(features::kMaterialDesignExtensions));
  // TODO(dbeam): remove hideSettingsAndHelp soon.
  source->AddBoolean("hideSettingsAndHelp", true);
  source->AddString("extensionsHost", chrome::kChromeUIExtensionsHost);
  source->AddLocalizedString("extensionsDisplayName",
                             IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);
  source->AddString("helpHost", chrome::kChromeUIHelpHost);
  source->AddLocalizedString("helpDisplayName", IDS_ABOUT_TITLE);
  source->AddString("settingsHost", chrome::kChromeUISettingsHost);
  source->AddLocalizedString("settingsDisplayName", IDS_SETTINGS_TITLE);

  source->DisableDenyXFrameOptions();
  source->OverrideContentSecurityPolicyChildSrc("child-src chrome:;");

  source->AddBoolean("profileIsGuest", profile->IsGuestSession());

  return source;
}

}  // namespace

SubframeLogger::SubframeLogger(content::WebContents* contents)
    : WebContentsObserver(contents) {}

SubframeLogger::~SubframeLogger() {}

void SubframeLogger::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  const GURL& url = navigation_handle->GetURL();
  if (url == chrome::kChromeUIExtensionsFrameURL ||
      url == chrome::kChromeUIHelpFrameURL ||
      url == chrome::kChromeUISettingsFrameURL ||
      url == chrome::kChromeUIUberFrameURL) {
    webui::LogWebUIUrl(url);
  }
}

UberUI::UberUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  subframe_logger_ = base::MakeUnique<SubframeLogger>(web_ui->GetWebContents());
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                CreateUberHTMLSource());

  RegisterSubpage(chrome::kChromeUIExtensionsFrameURL,
                  chrome::kChromeUIExtensionsHost);
  RegisterSubpage(chrome::kChromeUIHelpFrameURL,
                  chrome::kChromeUIHelpHost);
#if defined(OS_CHROMEOS)
  RegisterSubpage(chrome::kChromeUISettingsFrameURL,
                  chrome::kChromeUISettingsHost);
#endif
  RegisterSubpage(chrome::kChromeUIUberFrameURL,
                  chrome::kChromeUIUberHost);
}

UberUI::~UberUI() {
}

void UberUI::RegisterSubpage(const std::string& page_url,
                             const std::string& page_host) {
  sub_uis_[page_url] = web_ui()->GetWebContents()->CreateSubframeWebUI(
      GURL(page_url), page_host);
}

content::WebUI* UberUI::GetSubpage(const std::string& page_url) {
  if (!base::ContainsKey(sub_uis_, page_url))
    return nullptr;
  return sub_uis_[page_url].get();
}

void UberUI::RenderFrameCreated(RenderFrameHost* render_frame_host) {
  for (auto iter = sub_uis_.begin(); iter != sub_uis_.end(); ++iter) {
    iter->second->GetController()->RenderFrameCreated(render_frame_host);
  }
}

bool UberUI::OverrideHandleWebUIMessage(const GURL& source_url,
                                        const std::string& message,
                                        const base::ListValue& args) {
  // Find the appropriate subpage and forward the message.
  auto subpage = sub_uis_.find(source_url.GetOrigin().spec());
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
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context,
                                CreateUberFrameHTMLSource(browser_context));
}

UberFrameUI::~UberFrameUI() {
}
