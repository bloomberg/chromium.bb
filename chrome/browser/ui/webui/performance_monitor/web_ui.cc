// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/web_ui.h"

#include "base/values.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/performance_monitor/web_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

ChromeWebUIDataSource* CreateWebUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIPerformanceMonitorHost);

  source->add_resource_path("chart.css", IDR_PERFORMANCE_MONITOR_CHART_CSS);
  source->add_resource_path("chart.js", IDR_PERFORMANCE_MONITOR_CHART_JS);
  source->add_resource_path("jquery.js", IDR_PERFORMANCE_MONITOR_JQUERY_JS);
  source->add_resource_path("flot.js", IDR_PERFORMANCE_MONITOR_JQUERY_FLOT_JS);
  source->set_default_resource(IDR_PERFORMANCE_MONITOR_HTML);
  return source;
}

}  // namespace

namespace performance_monitor {

WebUI::WebUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new performance_monitor::WebUIHandler());

  ChromeWebUIDataSource* html_source = CreateWebUIHTMLSource();
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, html_source);
}

}  // namespace performance_monitor
