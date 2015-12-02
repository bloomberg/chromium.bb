// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_history_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/browsing_history_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/search.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history_login_handler.h"
#endif

namespace {

content::WebUIDataSource* CreateMdHistoryUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHistoryHost);

  // Localized strings (alphabetical order).
  source->AddLocalizedString("title", IDS_HISTORY_TITLE);

  // TODO(tsergeant): File resources (alphabetical order).

  source->SetDefaultResource(IDR_MD_HISTORY_HISTORY_HTML);
  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

MdHistoryUI::MdHistoryUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new BrowsingHistoryHandler());
  web_ui->AddMessageHandler(new MetricsHandler());

  // On mobile we deal with foreign sessions differently.
#if !defined(OS_ANDROID)
  if (search::IsInstantExtendedAPIEnabled()) {
    web_ui->AddMessageHandler(new browser_sync::ForeignSessionHandler());
    web_ui->AddMessageHandler(new HistoryLoginHandler());
  }
#endif

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateMdHistoryUIHTMLSource());
}

MdHistoryUI::~MdHistoryUI() {}

base::RefCountedMemory* MdHistoryUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_HISTORY_FAVICON, scale_factor);
}
