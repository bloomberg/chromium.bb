// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_scheduler_internals/task_scheduler_internals_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/task_scheduler_internals_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

std::unique_ptr<base::Value> SnapshotHistogramToValue(
    const base::HistogramBase* histogram) {
  std::unique_ptr<base::ListValue> values =
      base::MakeUnique<base::ListValue>();

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  std::unique_ptr<base::SampleCountIterator> iterator = samples->Iterator();
  while (!iterator->Done()) {
    base::HistogramBase::Sample min;
    base::HistogramBase::Sample max;
    base::HistogramBase::Count count;
    iterator->Get(&min, &max, &count);

    std::unique_ptr<base::DictionaryValue> bucket =
        base::MakeUnique<base::DictionaryValue>();
    bucket->SetInteger("min", min);
    bucket->SetInteger("max", max);
    bucket->SetInteger("count", count);

    values->Append(std::move(bucket));
    iterator->Next();
  }
  return std::move(values);
}

class TaskSchedulerDataHandler : public content::WebUIMessageHandler {
 public:
  TaskSchedulerDataHandler() = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getTaskSchedulerData",
        base::Bind(&TaskSchedulerDataHandler::GetTaskSchedulerData,
                   base::Unretained(this)));
  }

 private:
  void GetTaskSchedulerData(const base::ListValue*) {
    base::DictionaryValue data;

    base::TaskScheduler* task_scheduler = base::TaskScheduler::GetInstance();
    data.SetBoolean("instantiated", !!task_scheduler);

    if (task_scheduler) {
      std::unique_ptr<base::ListValue> histogram_value =
          base::MakeUnique<base::ListValue>();
      std::vector<const base::HistogramBase*> histograms =
          task_scheduler->GetHistograms();

      for (const base::HistogramBase* const histogram : histograms) {
        std::unique_ptr<base::DictionaryValue> buckets =
            base::MakeUnique<base::DictionaryValue>();
        buckets->SetString("name", histogram->histogram_name());
        buckets->Set("buckets", SnapshotHistogramToValue(histogram));
        histogram_value->Append(std::move(buckets));
      }

      data.Set("histograms", std::move(histogram_value));
    }

    AllowJavascript();
    CallJavascriptFunction(
        "TaskSchedulerInternals.onGetTaskSchedulerData", data);
  }

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerDataHandler);
};

}  // namespace

TaskSchedulerInternalsUI::TaskSchedulerInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<TaskSchedulerDataHandler>());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(
          chrome::kChromeUITaskSchedulerInternalsHost);
  html_source->AddResourcePath(
      "index.css", IDR_TASK_SCHEDULER_INTERNALS_RESOURCES_INDEX_CSS);
  html_source->AddResourcePath(
      "index.js", IDR_TASK_SCHEDULER_INTERNALS_RESOURCES_INDEX_JS);
  html_source->SetDefaultResource(
      IDR_TASK_SCHEDULER_INTERNALS_RESOURCES_INDEX_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

TaskSchedulerInternalsUI::~TaskSchedulerInternalsUI() = default;
