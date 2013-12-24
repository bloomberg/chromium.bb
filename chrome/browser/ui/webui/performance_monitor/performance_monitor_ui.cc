// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

content::WebUIDataSource* CreateWebUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPerformanceMonitorHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("chart.css", IDR_PERFORMANCE_MONITOR_CHART_CSS);
  source->AddResourcePath("chart.js", IDR_PERFORMANCE_MONITOR_CHART_JS);
  source->AddResourcePath("jquery.js", IDR_PERFORMANCE_MONITOR_JQUERY_JS);
  source->AddResourcePath("flot.js", IDR_PERFORMANCE_MONITOR_JQUERY_FLOT_JS);
  source->SetDefaultResource(IDR_PERFORMANCE_MONITOR_HTML);

  source->AddString("enableFlagsURL",
                    base::ASCIIToUTF16(chrome::kChromeUIFlagsURL));

  source->AddLocalizedString("title", IDS_PERFORMANCE_MONITOR_TITLE);
  source->AddLocalizedString("flagNotEnabledWarning",
                             IDS_PERFORMANCE_MONITOR_FLAG_NOT_ENABLED_WARNING);
  source->AddLocalizedString("enableFlag", IDS_PERFORMANCE_MONITOR_ENABLE_FLAG);
  source->AddLocalizedString("noAggregationWarning",
                             IDS_PERFORMANCE_MONITOR_NO_AGGREGATION_WARNING);
  source->AddLocalizedString("timeRangeSection",
                             IDS_PERFORMANCE_MONITOR_TIME_RANGE_SECTION);
  source->AddLocalizedString("timeResolutionCategory",
                             IDS_PERFORMANCE_MONITOR_TIME_RESOLUTION_CATEGORY);
  source->AddLocalizedString("timeLastFifteenMinutes",
                             IDS_PERFORMANCE_MONITOR_TIME_LAST_FIFTEEN_MINUTES);
  source->AddLocalizedString("timeLastHour",
                             IDS_PERFORMANCE_MONITOR_TIME_LAST_HOUR);
  source->AddLocalizedString("timeLastDay",
                             IDS_PERFORMANCE_MONITOR_TIME_LAST_DAY);
  source->AddLocalizedString("timeLastWeek",
                             IDS_PERFORMANCE_MONITOR_TIME_LAST_WEEK);
  source->AddLocalizedString("timeLastMonth",
                             IDS_PERFORMANCE_MONITOR_TIME_LAST_MONTH);
  source->AddLocalizedString("timeLastQuarter",
                             IDS_PERFORMANCE_MONITOR_TIME_LAST_QUARTER);
  source->AddLocalizedString("timeRangeButtonHeading",
                             IDS_PERFORMANCE_MONITOR_TIME_RANGE_BUTTON_HEADING);
  source->AddLocalizedString("aggregationCategory",
                             IDS_PERFORMANCE_MONITOR_AGGREGATION_CATEGORY);
  source->AddLocalizedString("metricsSection",
                             IDS_PERFORMANCE_MONITOR_METRICS_SECTION);
  source->AddLocalizedString("eventsSection",
                             IDS_PERFORMANCE_MONITOR_EVENTS_SECTION);
  source->AddLocalizedString("eventTimeMouseover",
                             IDS_PERFORMANCE_MONITOR_EVENT_TIME_MOUSEOVER);

  return source;
}

}  // namespace

namespace performance_monitor {

PerformanceMonitorUI::PerformanceMonitorUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new PerformanceMonitorHandler());

  content::WebUIDataSource* html_source = CreateWebUIHTMLSource();
  html_source->SetUseJsonJSFormatV2();

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

}  // namespace performance_monitor
