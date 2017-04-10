// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cleanup_tool/cleanup_tool_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cleanup_tool/cleanup_action_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

CleanupToolUI::CleanupToolUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUICleanupToolHost);

  // TODO(proberge): Localize strings once they are finalized.
  html_source->AddString("title", "Chrome Cleanup");
  html_source->AddString("sectionHeader",
                         "Remove suspicious or unwanted programs");
  html_source->AddString("scanAction", "Scan Now");
  html_source->AddString("scanning", "Scanning");
  html_source->AddString("cleanAction", "Run Chrome Cleanup");
  html_source->AddString("about", "About Chrome Cleanup");
  html_source->SetJsonPath("strings.js");

  html_source->AddResourcePath("cleanup_browser_proxy.html",
                               IDR_CLEANUP_TOOL_BROWSER_PROXY_HTML);
  html_source->AddResourcePath("cleanup_browser_proxy.js",
                               IDR_CLEANUP_TOOL_BROWSER_PROXY_JS);
  html_source->AddResourcePath("icons.html", IDR_CLEANUP_TOOL_ICONS_HTML);
  html_source->AddResourcePath("manager.html", IDR_CLEANUP_TOOL_MANAGER_HTML);
  html_source->AddResourcePath("manager.js", IDR_CLEANUP_TOOL_MANAGER_JS);
  html_source->AddResourcePath("toolbar.html", IDR_CLEANUP_TOOL_TOOLBAR_HTML);
  html_source->AddResourcePath("toolbar.js", IDR_CLEANUP_TOOL_TOOLBAR_JS);
  html_source->SetDefaultResource(IDR_CLEANUP_TOOL_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(base::MakeUnique<CleanupActionHandler>());
}

CleanupToolUI::~CleanupToolUI() {}
