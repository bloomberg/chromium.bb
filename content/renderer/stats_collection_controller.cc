// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/stats_collection_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_util.h"
#include "content/common/child_process_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using webkit_glue::CppArgumentList;
using webkit_glue::CppVariant;

namespace content {

namespace {

bool CurrentRenderViewImpl(RenderViewImpl** out) {
  WebKit::WebFrame* web_frame = WebKit::WebFrame::frameForCurrentContext();
  if (!web_frame)
    return false;

  WebKit::WebView* web_view = web_frame->view();
  if (!web_view)
    return false;

  RenderViewImpl* render_view_impl =
      RenderViewImpl::FromWebView(web_view);
  if (!render_view_impl)
    return false;

  *out = render_view_impl;
  return true;
}

// Encodes a WebContentsLoadTime as JSON.
// Input:
// - |load_start_time| - time at which page load started.
// - |load_stop_time| - time at which page load stopped.
// - |result| - returned JSON.
// Example return value:
// {'load_start_ms': 1, 'load_duration_ms': 2.5}
// either value may be null if a web contents hasn't fully loaded.
// load_start_ms is represented as milliseconds since system boot.
void ConvertLoadTimeToJSON(
    const base::TimeTicks& load_start_time,
    const base::TimeTicks& load_stop_time,
    std::string *result) {
  base::DictionaryValue item;

  if (load_start_time.is_null()) {
     item.Set("load_start_ms", base::Value::CreateNullValue());
  } else {
    // This code relies on an implementation detail of TimeTicks::Now() - that
    // its return value happens to coincide with the system uptime value in
    // microseconds, on Win/Mac/iOS/Linux/ChromeOS and Android.  See comments
    // in base::SysInfo::Uptime().
    item.SetDouble("load_start_ms", load_start_time.ToInternalValue() / 1000);
  }
  if (load_start_time.is_null() || load_stop_time.is_null()) {
    item.Set("load_duration_ms", base::Value::CreateNullValue());
  } else {
    item.SetDouble("load_duration_ms",
        (load_stop_time - load_start_time).InMilliseconds());
  }
  base::JSONWriter::Write(&item, result);
}

}  // namespace

StatsCollectionController::StatsCollectionController()
    : sender_(NULL) {
  BindCallback("getHistogram",
               base::Bind(&StatsCollectionController::GetHistogram,
                          base::Unretained(this)));
  BindCallback("getBrowserHistogram",
               base::Bind(&StatsCollectionController::GetBrowserHistogram,
                          base::Unretained(this)));
  BindCallback("tabLoadTiming",
               base::Bind(
                  &StatsCollectionController::GetTabLoadTiming,
                  base::Unretained(this)));
}

void StatsCollectionController::GetHistogram(const CppArgumentList& args,
                                            CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(args[0].ToString());
  std::string output;
  if (!histogram) {
    output = "{}";
  } else {
    histogram->WriteJSON(&output);
  }
  result->Set(output);
}

void StatsCollectionController::GetBrowserHistogram(const CppArgumentList& args,
                                                   CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }

  if (!sender_) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  std::string histogram_json;
  sender_->Send(new ChildProcessHostMsg_GetBrowserHistogram(
      args[0].ToString(), &histogram_json));
  result->Set(histogram_json);
}

void StatsCollectionController::GetTabLoadTiming(
    const CppArgumentList& args,
    CppVariant* result) {
  if (!sender_) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  RenderViewImpl *render_view_impl = NULL;
  if (!CurrentRenderViewImpl(&render_view_impl)) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  StatsCollectionObserver* observer =
      render_view_impl->GetStatsCollectionObserver();
  if (!observer) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  std::string tab_timing_json;
  ConvertLoadTimeToJSON(
      observer->load_start_time(), observer->load_stop_time(),
      &tab_timing_json);
  result->Set(tab_timing_json);
}

}  // namespace content
