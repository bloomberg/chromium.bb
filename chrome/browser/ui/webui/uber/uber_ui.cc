// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/uber/uber_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/options2/options_ui.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"

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

UberUI::UberUI(TabContents* contents) : ChromeWebUI(contents) {
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(CreateUberHTMLSource());

  ChromeWebUI* options = new Options2UI(contents);
  options->set_frame_xpath("//iframe[@id='settings']");
  sub_uis_.push_back(options);
}

UberUI::~UberUI() {
}

void UberUI::RenderViewCreated(RenderViewHost* render_view_host) {
  for (size_t i = 0; i < sub_uis_.size(); i++) {
    sub_uis_[i]->RenderViewCreated(render_view_host);
  }

  ChromeWebUI::RenderViewCreated(render_view_host);
}

void UberUI::RenderViewReused(RenderViewHost* render_view_host) {
  for (size_t i = 0; i < sub_uis_.size(); i++) {
    sub_uis_[i]->RenderViewReused(render_view_host);
  }

  ChromeWebUI::RenderViewReused(render_view_host);
}

void UberUI::DidBecomeActiveForReusedRenderView() {
  for (size_t i = 0; i < sub_uis_.size(); i++) {
    sub_uis_[i]->DidBecomeActiveForReusedRenderView();
  }

  ChromeWebUI::DidBecomeActiveForReusedRenderView();
}

void UberUI::OnWebUISend(const GURL& source_url,
                         const std::string& message,
                         const ListValue& args) {
  // TODO(estade): This should only send the message to the appropriate
  // subpage (if any), not all of them.
  for (size_t i = 0; i < sub_uis_.size(); i++) {
    sub_uis_[i]->OnWebUISend(source_url, message, args);
  }

  ChromeWebUI::OnWebUISend(source_url, message, args);
}
