// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_scheduler_internals/task_scheduler_internals_ui.h"

#include "base/task_scheduler/task_scheduler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/task_scheduler_internals_resources.h"
#include "content/public/browser/web_ui_data_source.h"

TaskSchedulerInternalsUI::TaskSchedulerInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(
          chrome::kChromeUITaskSchedulerInternalsHost);

  html_source->AddString("status", base::TaskScheduler::GetInstance()
      ? "Instantiated"
      : "Not Instantiated");

  html_source->SetDefaultResource(
      IDR_TASK_SCHEDULER_INTERNALS_RESOURCES_INDEX_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

TaskSchedulerInternalsUI::~TaskSchedulerInternalsUI() = default;
