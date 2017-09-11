// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sys_internals/sys_internals_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sys_internals/sys_internals_message_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

SysInternalsUI::SysInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<SysInternalsMessageHandler>());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUISysInternalsHost);
  html_source->AddResourcePath("index.css", IDR_SYS_INTERNALS_CSS);
  html_source->AddResourcePath("index.js", IDR_SYS_INTERNALS_JS);
  html_source->AddResourcePath("line_chart.css",
                               IDR_SYS_INTERNALS_LINE_CHART_CSS);
  html_source->AddResourcePath("line_chart.js",
                               IDR_SYS_INTERNALS_LINE_CHART_JS);
  html_source->SetDefaultResource(IDR_SYS_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

SysInternalsUI::~SysInternalsUI() {}

// static
bool SysInternalsUI::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kSysInternals);
}
