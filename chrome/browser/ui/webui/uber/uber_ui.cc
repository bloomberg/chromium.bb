// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/uber/uber_ui.h"

#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"

using content::WebContents;

namespace {

ChromeWebUIDataSource* CreateUberHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIUberHost);

  source->set_json_path("strings.js");
  source->add_resource_path("uber.js", IDR_UBER_JS);
  source->set_default_resource(IDR_UBER_HTML);
  return source;
}

}  // namespace

UberUI::UberUI(WebContents* contents) : ChromeWebUI(contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  profile->GetChromeURLDataManager()->AddDataSource(CreateUberHTMLSource());

  RegisterSubpage(chrome::kChromeUISettingsFrameURL);
  RegisterSubpage(chrome::kChromeUIExtensionsFrameURL);
}

UberUI::~UberUI() {
  STLDeleteValues(&sub_uis_);
}

void UberUI::RegisterSubpage(const std::string& page_url) {
  ChromeWebUI* web_ui = static_cast<ChromeWebUI*>(
      ChromeWebUIFactory::GetInstance()->CreateWebUIForURL(
          static_cast<TabContents*>(web_contents_), GURL(page_url)));

  web_ui->set_frame_xpath("//iframe[@src='" + page_url + "']");
  sub_uis_[page_url] = web_ui;
}

void UberUI::RenderViewCreated(RenderViewHost* render_view_host) {
  for (SubpageMap::iterator iter = sub_uis_.begin(); iter != sub_uis_.end();
       ++iter) {
    iter->second->RenderViewCreated(render_view_host);
  }

  ChromeWebUI::RenderViewCreated(render_view_host);
}

void UberUI::RenderViewReused(RenderViewHost* render_view_host) {
  for (SubpageMap::iterator iter = sub_uis_.begin(); iter != sub_uis_.end();
       ++iter) {
    iter->second->RenderViewReused(render_view_host);
  }

  ChromeWebUI::RenderViewReused(render_view_host);
}

void UberUI::DidBecomeActiveForReusedRenderView() {
  for (SubpageMap::iterator iter = sub_uis_.begin(); iter != sub_uis_.end();
       ++iter) {
    iter->second->DidBecomeActiveForReusedRenderView();
  }

  ChromeWebUI::DidBecomeActiveForReusedRenderView();
}

void UberUI::OnWebUISend(const GURL& source_url,
                         const std::string& message,
                         const ListValue& args) {
  // Find the appropriate subpage and forward the message.
  SubpageMap::iterator subpage = sub_uis_.find(source_url.GetOrigin().spec());
  if (subpage == sub_uis_.end()) {
    // The message was sent from the uber page itself.
    DCHECK_EQ(std::string(chrome::kChromeUIUberHost), source_url.host());
    ChromeWebUI::OnWebUISend(source_url, message, args);
  } else {
    // The message was sent from a subpage.
    subpage->second->OnWebUISend(source_url, message, args);
  }
}
